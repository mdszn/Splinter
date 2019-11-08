#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <error.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <setjmp.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <poll.h>
#include <fcntl.h>
#include <termios.h>

#include "splinter.h"
#include "connectioninfo.h"
#include "server.h"
#include "thread_read.h"

#define _POSIX_SOURCE 1
#define BUFSIZE 4096
#define NAMESIZE 20

sig_atomic_t term;
sigjmp_buf jump;

void sig_hand(int i)
{
  term = 1;
  siglongjmp(jump, 0);
}

int server_start(int argc, char* argv[])
{
  int sock;
  int rc;
  int nread;
  int backlog = 10;
  int peer;
  char* buf;
  char uname[NAMESIZE];
  struct server *server = 0;
  pid_t pid;
  pthread_t tid;

  signal(SIGINT, sig_hand);
  signal(SIGTERM, sig_hand);
  signal(SIGUSR1, sig_hand);

  server = alloc_serverinfo();
  getconnectioninfo(server, argc, argv);

  sock = -1;

  buf = malloc(BUFSIZE);
  if (!buf) {
		return 0;
  }
  memset(buf, 0, BUFSIZE);

	printf("%s\n", port(server));
	printf("%s\n", host(server));

	sock = s_bind(host(server), port(server));
  if (sock < 0) {
		printf("error on bind() when binding to host and port\n");
		return 0;
  }

  rc = s_listen(sock, backlog);
  if (rc < 0) {
		printf("Listen() returned when listening to sock.\n");
		return 0;
  }

  fprintf(stderr, "pid: %d\n", getpid());
  // when server recieves signal return to start of loop to terminate
  sigsetjmp(jump, 0);
  while (!term) {
    peer = s_accept(sock);
		if (peer > 0) {
			printf("connected\n");
      // get user name from new connection
      if((nread = read(peer, buf, BUFSIZE)) < 0)
        error(EXIT_FAILURE, errno, "read failed");
      strncpy(uname, buf, NAMESIZE);

      if((pid = fork()) < 0) {
        error(EXIT_FAILURE, errno, "fork failed");
      }
      else if(pid == 0) {
          // fork off new process to create psuedoterminal
          // and exec a shell for the client
          create_pty(peer, uname);
          exit(0);
      }
      else {
        // create a thread to wait for the child
        pthread_create(&tid, NULL, &thrd_wait, (void *)&pid);
        pthread_detach(tid);
        sleep(1);
        close(peer);
      }
    }
		else{
      break;
    }
  }
  fprintf(stderr, "good-bye.\n");

  if(sock > 0)
    close(sock);

  if(buf)
    free(buf);

	if(server != 0)
		free(server);

  return 0;
}

void
create_pty(int peer, char *uname)
{
  int ptyslave, ptymaster;
  char name[50], *nameptr;
  char *shell_args[2] = {uname, NULL};
  pid_t pid;
  struct descriptors *fds;
  pthread_t tidp1, tidp2;

  // open PTY
  if((ptymaster = posix_openpt(O_RDWR | O_NOCTTY)) < 0) {
    // error in opening pty
    close(peer);
    error(EXIT_FAILURE, errno, "failure when opening pty");
  }

  if(grantpt(ptymaster) < 0) {
    close(peer);
    error(EXIT_FAILURE, errno, "grantpt failed");
  }

  if(unlockpt(ptymaster) < 0) {
    close(peer);
    error(EXIT_FAILURE, errno, "unlockpt failed");
  }

  // Get name of pty master opened
  if((nameptr = ptsname(ptymaster)) == NULL) {
    error(EXIT_FAILURE, errno, "failed to obtain name of pty-master");
  }
  strncpy(name, nameptr, 50);

  if((pid = fork()) < 0) {
    error(EXIT_FAILURE, errno, "fork failed");
  }
  else if(pid == 0)
  {
    struct termios st;
    // child opens pty slave
    setsid();
    if((ptyslave = open(name, O_RDWR)) < 0) {
      error(EXIT_FAILURE, errno, "failed to open pty slave");
    }
    // close the master fd and reroute all regular I/O fds to the slave fd
    close(ptymaster);
    if(dup2(ptyslave, STDIN_FILENO) < 0)
      error(EXIT_FAILURE, errno, "dup2 failed");
    if(dup2(ptyslave, STDOUT_FILENO) < 0)
      error(EXIT_FAILURE, errno, "dup2 failed");
    if(dup2(ptyslave, STDERR_FILENO) < 0)
      error(EXIT_FAILURE, errno, "dup2 failed");

    // Turn off echoing -
    // noecho function from Advanced Programming in the Linux Environment pg731
    if(tcgetattr(ptyslave, &st) < 0)
      error(EXIT_FAILURE, errno, "get termio attr failed");
    st.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
    st.c_oflag &= ~(ONLCR);
    if(tcsetattr(ptyslave, TCSANOW, &st) < 0)
      error(EXIT_FAILURE, errno, "set termio attr failed");
    // Ready to exec the shell, all communication is routed throught the pty
    execv("./shell", shell_args);
    error(EXIT_FAILURE, errno, "exec shell failed in child");
  }
  else
  {
    // Master process
    // passes input from client into pty
    if((fds = malloc(sizeof(struct descriptors))) < 0)
      error(EXIT_FAILURE, errno, "malloc failed");
    fds->read_in = peer;
    fds->write_out = ptymaster;
    if(pthread_create(&tidp1, NULL, thrd_reader, (void *)fds) != 0)
      error(EXIT_FAILURE, errno, "thread creation failed");

    // passes output from pty to the client
    fds = malloc(sizeof(struct descriptors));
    fds->read_in = ptymaster;
    fds->write_out = peer;
    if(pthread_create(&tidp2, NULL, thrd_reader, (void *)fds) < 0)
      error(EXIT_FAILURE, errno, "thread creation failed");
    waitpid(pid, 0, 0);
    pthread_cancel(tidp1);
    pthread_cancel(tidp2);
  }
  close(ptymaster);
  return;
}
