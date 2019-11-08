#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <error.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "splinter.h"
#include "connectioninfo.h"

#define _POSIX_SOURCE 1
#define _XOPEN_SOURCE 600

int
s_listen(int socketfd, int bl)
{
	int rc;
	rc = listen(socketfd, bl);
	return rc;
}

int
s_bind(const char *host, const char *port)
{
	int sock = -1; //By Default Lets Fail
	int rc;
	struct addrinfo pull;
	struct addrinfo *address;

	printf("%s -> %s\n", host, port);

	memset(&pull, 0, sizeof pull);
  pull.ai_family = AF_INET;
  pull.ai_socktype = SOCK_STREAM;
  pull.ai_flags = AI_PASSIVE;

	if(!host || !port) {
		printf("Please supply a Host or Port");
		return -1;
	}

	rc = getaddrinfo(host, port, &pull, &address);

	if(rc != 0) {
		printf("Error On getaddrinfo();");
		return -1;
	}

	struct addrinfo *i;

	for(i = address; i != NULL; ++i) {

  	sock = socket(i->ai_family, i->ai_socktype, i->ai_protocol);

	  if (sock < 0)
			continue;

	  rc = bind(sock, i->ai_addr, i->ai_addrlen);

	  if (rc < 0) {
			error(EXIT_FAILURE, errno, "bind failed");
		  return -1;
  	}
		else {
		  struct sockaddr_in *socket_address = (struct sockaddr_in*)i->ai_addr;
		  const char *host_name = 0;
		  int port_num = 0;
		  char* buf;
		  buf = malloc(1024);
		  if (buf) {
			  memset(buf, 0, 1024);
			  host_name = inet_ntop(i->ai_family, &socket_address->sin_addr, buf, 1024);
			  port_num = ntohs(socket_address->sin_port);
				printf("Binded Successfuly With Host: %s Port: %d)\n", host_name, port_num);
			  free(buf);
  		}
  		break;
  	}
	}

	freeaddrinfo(address);

	return sock;
}

int
s_connect(const char *host, const char *port, int sockettype)
{
	int sock = -1; //By Default Lets Fail
	int rc;
	struct addrinfo pull;
	struct addrinfo *address;

	memset(&pull, 0, sizeof pull);
  pull.ai_family = AF_INET;
  pull.ai_socktype = sockettype;

	if(!host || !port) {
		printf("Please supply a Host or Port");
		return -1;
	}

	rc = getaddrinfo(host, port, &pull, &address);

	if(rc != 0) {
		printf("Error On getaddrinfo();");
		return -1;
	}

	struct addrinfo *i;
	for(i = address; i != NULL; ++i) {
  	sock = socket(i->ai_family, i->ai_socktype, i->ai_protocol);

	  if (sock < 0)
			continue;

	  rc = connect(sock, i->ai_addr, i->ai_addrlen);
	  if (rc < 0) {
			return -1;
  	}
		else {
  		break;
  	}
	}

	freeaddrinfo(address);

	return sock;
}

int
s_accept(int sockfd)
{
	int peer;
  struct sockaddr_in peer_socketadd;
  socklen_t peer_socketadd_size = sizeof(struct sockaddr_in);
  peer = accept(sockfd, (struct sockaddr*)&peer_socketadd, &peer_socketadd_size);
  if (peer > 0) {
		printf("Accepted\n");
  }

	return peer;
}
