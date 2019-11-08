#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/acct.h>
#include <sys/time.h>
#include <error.h>
#include <errno.h>
#include <glob.h>

#define LINEMAX 4096
#define PROMPT_SIZE 100
#define FALSE 0
#define TRUE 1

typedef int funcPtr(char *);

int USE_ACCOUNTING = FALSE;
int doGlob = 1;

int
exitbuiltin() {
  exit(EXIT_SUCCESS);
}

int
globon() {
  doGlob = 1;
  return 0;
}

int
globoff() {
  doGlob = 0;
  return 0;
}

typedef struct {
  char *cmd;
  funcPtr *name;
} dTable;

dTable fdt[] = {
  {"_exit", exitbuiltin},
  {"_globon", globon},
  {"_globoff", globoff}
};

void
dobuiltin(char *cmd) {
  for(int i = 0; i < 3; ++i) {
    if(strncmp(cmd, fdt[i].cmd, strlen(fdt[i].cmd)) == 0)
      (*fdt[i].name)(cmd);
  }
}

static void
Error(int status, int errnum, char *msg)
{
  char errmsg[LINEMAX];
  char *errString;
  char *sep = ": ";

  errString = strerror(errnum);
  // program_invocation_name not portable, requires the _GNU_SOURCE definiton
  strcpy(errmsg, program_invocation_name);
  strcat(errmsg, sep);
  strcat(errmsg, msg);
  strcat(errmsg, sep);
  strcat(errmsg, errString);
  strcat(errmsg, "\n");
  write(STDERR_FILENO, errmsg, strlen(errmsg));

  exit(status);
}

void
globify(char **args, glob_t *globber)
{
    int nargs = 0;
    char **ptr;
    ptr = args;
    while(*ptr)
    {
      ++nargs;
      ++ptr;
    }

    if(nargs == 0)
      return;

    globber->gl_offs = 0;
    if(glob(args[0],
            GLOB_NOCHECK | GLOB_TILDE | GLOB_NOMAGIC,
            NULL, globber) < 0)
    {
        Error(EXIT_FAILURE, errno, "glob failed");
    }
    for(int i = 1; i < nargs; ++i)
    {
        glob(args[i],
             GLOB_NOCHECK | GLOB_TILDE | GLOB_NOMAGIC | GLOB_APPEND,
             NULL, globber);
    }

    return;
}

// Malloc for strings with error handling
static void
strMalloc(char **buf, uint size)
{
  *buf = malloc(size);
  if (buf == NULL)
    Error(EXIT_FAILURE, 0, "malloc failed");
}

// malloc for arrays of strings with error handling
static void
strStarMalloc(char ***buf, uint size)
{
  *buf = malloc(size);
  if (buf == NULL)
    Error(EXIT_FAILURE, 0, "malloc failed");
}

void
setResource(char *buf) {
  char alt[strlen(buf) + 1];
  char *p;
  USE_ACCOUNTING = TRUE;
  strcpy(alt, buf);
  p = alt + strlen("rs ");
  while(*p != '\0' && *p == ' ') // chomp spaces
    ++p;
  strcpy(buf, p);
}

void
rsPrint(struct rusage *r) {
  int maxLen, len;
  char ** p;
  char *prompts[] = {"User time", "System time", "Max size",
    "Shared text memory", "Unshared memory", "Unshared stack space",
    "Page fault (No I/O)", "Page fault", "Times swapped",
    "Reads from disk", "Writes to disk", "IPC messages sent",
    "IPC messages recieved", "Signals recieved",
    "Voluntary context switches", "Involuntary context switches", NULL};
  p = prompts;
  maxLen = strlen(*p++);
  while(*p) {
    len = strlen(*p++);
    if(len > maxLen)
      maxLen = len;
  }
  maxLen += 2;
  p = prompts;
  printf("%-*s %ld usec\n", maxLen, *p++, r->ru_utime.tv_usec);
  printf("%-*s %ld usec\n", maxLen, *p++, r->ru_utime.tv_usec);
  printf("%-*s %ld\n", maxLen, *p++, r->ru_maxrss);
  printf("%-*s %ld\n", maxLen, *p++, r->ru_ixrss);
  printf("%-*s %ld\n", maxLen, *p++, r->ru_idrss);
  printf("%-*s %ld\n", maxLen, *p++, r->ru_isrss);
  printf("%-*s %ld\n", maxLen, *p++, r->ru_minflt);
  printf("%-*s %ld\n", maxLen, *p++, r->ru_majflt);
  printf("%-*s %ld\n", maxLen, *p++, r->ru_nswap);
  printf("%-*s %ld\n", maxLen, *p++, r->ru_inblock);
  printf("%-*s %ld\n", maxLen, *p++, r->ru_oublock);
  printf("%-*s %ld\n", maxLen, *p++, r->ru_msgsnd);
  printf("%-*s %ld\n", maxLen, *p++, r->ru_msgrcv);
  printf("%-*s %ld\n", maxLen, *p++, r->ru_nsignals);
  printf("%-*s %ld\n", maxLen, *p++, r->ru_nvcsw);
  printf("%-*s %ld\n", maxLen, *p++, r->ru_nivcsw);
}

// Trims whitespace from both ends of a string
// "    ls -l -F      " -> "ls -l -F"
void
trim(char **str) {
    char *p;
    char cpy[strlen(*str) + 1];
    int length;

    strcpy(cpy, *str);
    p = cpy;
    while(*p == ' ' && *p != '\0')
      ++p;
    length = strlen(p) - 1;
    if(p[length] == ' ') {
      while(p[length] == ' ') {
        --length;
      }
      p[++length] = '\0';
    }
    strcpy(*str, p);
}

// Returns the number of pipes in an entered command
int
countPipes(char *buf) {
  int count = 0;
  int length = strlen(buf);
  for(int i = 0; i < length; ++i) {
    if(buf[i] == '|')
      ++count;
  }
  return count;
}

// Extract individual commands from a pipeline
// cat| foo | bar will return an array of strings
// "cat", "foo", "bar", NULL
// return array is null terminated
char**
pipeify(char *buf)
{
    char cpy[strlen(buf) + 1];
    char *token;
    char **pipedCmds;
    char **p;
    int count;

    strcpy(cpy, buf);
    count = countPipes(buf);
    strStarMalloc(&pipedCmds, sizeof(char*) * (count + 2));
    pipedCmds[count + 1] = NULL;
    p = pipedCmds;
    token = strtok(cpy, "|");

    while(token) {
      strMalloc(p, sizeof(char) * (strlen(token) + 1));
      strcpy(*p, token);
      trim(p);
      ++p;
      token = strtok(NULL, "|");
    }
    return pipedCmds;
}

// Returns true if the command begins with a / ./ or ../
// Returns false otherwise
static int
directPath(char *buf)
{
  int length = strlen(buf);
  if(length == 0)
    return 1;
  if(buf[0] == '/') {
    return 1;
  }
  else if(buf[0] == '.') {
    if(length > 1) {
      if(buf[1] == '/')
        return 1;
      else if(buf[1] == '.')
        if(length > 2 && buf[2] == '/')
          return 1;
    }
  }
  return 0;
}

// Splits up a buffered command into substrings to isolate optional
// argument flags from the base command
// e.g. "ls -l" becomes ["ls", "-l", NULL]
// Return: Pointer to array of dynamically allocated strings
static char**
parseArgs(char *buf)
{
  char *tok; // strtok token
  char *delimiter = " ";
  int numArgs = 1; // total number of arguments from buffer
  char *buf2;
  char **list;

  // Find number of arguments to size list
  strMalloc(&buf2, sizeof(char) * (strlen(buf) + 1));
  strcpy(buf2, buf);
  strtok(buf2, delimiter);
  while(strtok(NULL, delimiter) != NULL)
    ++numArgs;
  free(buf2);
  // Size of argument list is the number of args + the NULL at the end
  strStarMalloc(&list, sizeof(char*) * (numArgs + 1));

  // Extract arguments
  tok = strtok(buf, delimiter);
  for(int i = 0; i < numArgs; ++i) {
    strMalloc(&list[i], strlen(tok) + 1);
    strcpy(list[i], tok);
    tok = strtok(NULL, delimiter);
  }
  list[numArgs] = NULL;

  return list;
}

void
freeList(char **optList)
{
  char **ptr;
  ptr = optList;
  while(*ptr)
  {
    free(*ptr);
    ++ptr;
  }
  free(optList);
}
// Concatenates p1 with p2 into path, adding a seperator (/) between
static void
createPath(char *path, char *p1, char *p2)
{
  strcpy(path, p1);
  strcat(path, "/");
  strcat(path, p2);
}

void
Exec(char *buf)
{
  char * path; // PATH environment variable
  char *tok; // path tokens returned from strok
  char *cmd; // command to be executed by exec
  char *delim = ":"; // delimiter used by strtok for path
  char **optList; // arguments for execv
  unsigned int length;
  glob_t gl;

  optList = parseArgs(buf);
  if(doGlob)
    globify(optList, &gl);
  if(directPath(optList[0])) {
    execv(optList[0], doGlob ? gl.gl_pathv : optList);
  }
  else {
    length = strlen(optList[0]);
    strMalloc(&path, sizeof(char) * (strlen(getenv("PATH")) + 1));
    strcpy(path, getenv("PATH"));
    tok = strtok(path, delim);

    while(tok != NULL) {
      // total size = size of buffer command + '/' +  size of path token + '\0'
      strMalloc(&cmd, sizeof(char) * (length + strlen(tok) + 2));
      createPath(cmd, tok, optList[0]);
      if(access(cmd, F_OK) == 0)
        execv(cmd, doGlob ? gl.gl_pathv : optList);

      free(cmd);
      tok = strtok(NULL, delim);
    }
    free(path);
  }
  freeList(optList);
  if(doGlob)
    globfree(&gl);
}

static int
Fork()
{
  pid_t pid;
  if ((pid = fork()) < 0)
    Error(EXIT_FAILURE, errno, "fork error");
  return(pid);
}

void
Dup2(int ofd, int nfd)
{
  if(dup2(ofd, nfd) == -1)
    Error(EXIT_FAILURE, errno, "dup2 failed");
}

void
Pipe(int *fds)
{
    if(pipe(fds) < 0)
      Error(EXIT_FAILURE, errno, "pipe failed");
}

void
Chdir(char *path)
{
    trim(&path);
    if(chdir(path) < 0)
    {
      perror(strerror(errno));
    }
}

void
pipeExec(char ** cmds, int npipes)
{
  // Code derived from pseudocode from Christopher Neylan
  // https://stackoverflow.com/questions/8389033/implementation-of-multiple-pipes-in-c
  // Exec on pipelined commands
  int pid, cmdIdx;
  char **cmd = cmds;
  int pipefds[npipes * 2];
  struct rusage resource;
  int status;

  for(int i = 0; i < npipes; ++i) {
    Pipe(pipefds + i * 2);
  }

  cmdIdx = 0;
  while(*cmd) {
    pid = Fork();
    if(pid == 0) {
      if(cmdIdx != 0) {
        // not the first command
        Dup2(pipefds[(cmdIdx - 1) * 2], 0);
      }
      if(cmdIdx != npipes) {
        // not the last command
        Dup2(pipefds[cmdIdx * 2 + 1], 1);
      }
      for(int i = 0; i < npipes * 2; ++i) {
        close(pipefds[i]);
      }
      Exec(*cmd);
    }
    ++cmdIdx;
    ++cmd;
  }
  for(int i = 0; i < npipes * 2; ++i) {
    close(pipefds[i]);
  }
  for(int i = 0; i < cmdIdx; ++i) {
    if(USE_ACCOUNTING) {
      if(wait3(&status, 0, &resource) == -1)
        Error(EXIT_FAILURE, errno, "wait failed");
    }
    else {
      if(wait(0) == -1)
        Error(EXIT_FAILURE, errno, "wait failed");
    }
  }
  if(USE_ACCOUNTING) {
    rsPrint(&resource);
    USE_ACCOUNTING = FALSE;
  }
}

void
makePrompt(char *prompt, char *name)
{
    char cwd[LINEMAX];
    getcwd(cwd, LINEMAX);
    sprintf(prompt, "%s: %s $ ", name, cwd);
}

int
main(int argc, char **argv)
{
  char buf[LINEMAX];
  char *name;
  char **pipes, **ptr, **args;
  pid_t pid;
  int status;
  int n;
  int count;
  char prompt[PROMPT_SIZE];
  struct rusage resource;
  glob_t gl;

  if(argc > 0)
    name = argv[0];

  do {
    makePrompt(prompt, name);
    write(STDOUT_FILENO, prompt, strlen(prompt));
    memset(buf, 0, LINEMAX);
    if((n = read(STDIN_FILENO, buf, LINEMAX)) == 0) {
      printf("use exit to exit shell\n");
      continue;
    }
    buf[strlen(buf) - 1] = '\0'; // chomp '\n'

    if(strncmp(buf, "rs ", 3) == 0) {
      setResource(buf);
    }

    if((count = countPipes(buf)) > 0) {
      pipes = pipeify(buf);
      pipeExec(pipes, count);
      ptr = pipes;
      while(*ptr)
        free(*ptr++);
      free(pipes);
      continue;
    }

    if(buf[0] == '_') {
      dobuiltin(buf);
      continue;
    }

    if(strcmp(buf, "exit") == 0)
    {
        break;
    }
    else if(strncmp(buf, "cd", 2) == 0)
    {
      args = parseArgs(buf);
      if(args[1] == NULL)
      {
        // just have ["cd", NULL], go to home dir
        Chdir(getenv("HOME"));
      }
      else if(args[2] == NULL)
      {
        // only have two arguments, change dir to second arg
        if(doGlob)
          globify(args, &gl);
        Chdir(gl.gl_pathv[1]);
        if(doGlob)
          globfree(&gl);
      }
      else
      {
        Error(0, errno, "too many arguments to cd");
      }
      freeList(args);
      continue;
    }
    // Run other commands
    pid = Fork();
    if (pid == 0) {  // child
      Exec(buf);
      Error(EXIT_FAILURE, errno, "exec failure");
    }
    // parent
    if(USE_ACCOUNTING) {
      if ((pid = wait4(pid, &status, 0, &resource)) < 0)
        Error(EXIT_FAILURE, errno, "waitpid error");
      USE_ACCOUNTING = FALSE;
      rsPrint(&resource);
    }
    else {
      if ((pid = waitpid(pid, &status, 0)) < 0)
        Error(EXIT_FAILURE, errno, "waitpid error");
    }

  } while(1);

  return 0;
}
