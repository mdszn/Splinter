#ifndef SPLINTER_H_
#define SPLINTER_H_
int s_bind(const char *host, const char *port);
int s_connect(const char *host, const char *port, int socketttype);
int s_listen(int, int);
int s_accept(int sockfd);
#endif
