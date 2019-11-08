#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <wait.h>
#include <signal.h>

#include "thread_read.h"

#define BUFSIZE 4096

void
clean_thrd(void *arg)
{
  free(arg);
}

void *
thrd_reader(void *arg)
{
  struct descriptors fds = *(struct descriptors *)arg;
  int nread;
  int err = 0;
  char buffer[BUFSIZE];
  pthread_cleanup_push(clean_thrd, arg);
  while(1) {
    if((nread = read(fds.read_in, buffer, BUFSIZE)) < 0) {
        err = 1;
        break;
    }
    else if(nread == 0) {
      // eof detected
      break;
    }
    write(fds.write_out, buffer, nread);
    memset(buffer, 0, BUFSIZE);
  }
  pthread_cleanup_pop(1);
  if(err)
    pthread_exit((void *) -1);
  else
    pthread_exit((void *) 0);
}

void*
thrd_wait(void * arg)
{
  int pid = *(int *)arg;
  waitpid(pid, NULL, 0);
  pthread_exit(NULL);
}
