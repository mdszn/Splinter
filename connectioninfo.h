#ifndef CONNECTIONINFO_H_
#define CONNECTIONINFO_H_
struct server;

struct server *alloc_serverinfo(void);
int getconnectioninfo(struct server *server, int argc, char *argv[]);
int setparams(struct server *server, int opt, char *argv);
const char *host(struct server *server);
const char *port(struct server *server);
int serverresponse(int server_fd);
#endif
