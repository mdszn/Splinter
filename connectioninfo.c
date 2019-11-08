#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <poll.h>
#include "connectioninfo.h"

#define LINEMAX 4096

#define DEAFULT_HOST "192.168.0.4"
#define DEAFULT_PORT "9000"

static const char* options = "a:p:";

struct server {
    const char* host;
    const char* port;
};

struct server *
alloc_serverinfo(void)
{
    struct server* server;
    server = malloc(sizeof(struct server));
    if (server != 0) {
        memset(server, 0, sizeof *server);
        server->host = DEAFULT_HOST;
        server->port = DEAFULT_PORT;
    }
    return server;
}

int
getconnectioninfo(struct server *server, int argc, char *argv[])
{
	int optc;

	if(!server)
		return -1;

	while((optc = getopt(argc, argv, options)) != -1) {
		if('?' == optc) {
			optc = optopt;
		}
		setparams(server, optc, optarg);
	}
	return 0;
}

int
setparams(struct server *server, int opt, char *arg)
{

	if(!server)
		return -1;

	switch(opt) {
		case 'a':	server->host = arg; break;
		case 'p': server->port = arg; break;
		default: break;
	}

	return 0;
}

const char *
host(struct server *server)
{
	const char *host = 0;
	if(server)
		host = server->host;

	return host;
}

const char *
port(struct server *server)
{
	const char *port = 0;
	if(server)
		port = server->port;

	return port;
}

int
serverresponse(int server_fd)
{
	int rc = 0;
	int finished = 0;
	int buffersize = LINEMAX;
	int timeout = 5000;
	char *buffer;
	struct pollfd server;

	server.fd = server_fd;
	server.events = POLLIN;
	server.revents = 0;

	buffer = malloc(buffersize);

	if(!buffer)
		return -1;

	while(!finished) {
		int poll_server;

		poll_server = poll(&server, 1, timeout);

		if(poll_server == 0) {
			printf("Error serverresponse(), poll returned 0, timed out");
			finished = 1;
		}

		if(poll_server < 0) {
			printf("Error serverresponse(), poll returned greater then 1");
			finished = 1;
		}

		else {
			int server_r = 0;
			memset(buffer, 0, buffersize);
			server_r = read(server_fd, buffer, buffersize - 1);
			if(server_r > 0) {
				fprintf(stdout, "%s", buffer);
				finished = 1;
			}

			else {
				printf("Issue with read in serverresponse()");
				finished = 1;
			}
		}
	}

	free(buffer);

	return rc;
}
