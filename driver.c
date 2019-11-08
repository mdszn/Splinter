#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "client.h"
#include "server.h"

int main(int argc, char **argv)
{
  if(argc == 1) {
    printf("Usage: ./splinter <start|connect> -a <ip address> -p <port>\n");
  }
  else if(argc > 1)
  {
    if(strcmp(argv[1], "start") == 0) {
      server_start(argc, argv);
    }
    else if(strcmp(argv[1], "connect") == 0) {
      connect_server(argc, argv);
    }
    else {
      printf("Unknown option %s.\n", argv[1]);
      printf("Usage: /splinter <start|connect> -a <ip address> -p <port>\n");
    }
  }
  return 0;
}
