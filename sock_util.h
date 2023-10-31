#ifndef SOCK_UTIL_H
#define SOCK_UTIL_H	1

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>

/* default backlog for listen() system call */
#define DEFAULT_LISTEN_BACKLOG 5

int connect_to_tcp_server(const char *name, unsigned short port);
int close_tcp_connection(int s);
int create_listener(unsigned short port, int backlog, int type);
int disableNagle(int sockfd);

#endif
