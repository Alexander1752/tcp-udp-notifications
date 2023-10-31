#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include "util.h"
#include "sock_util.h"
#include "common.h"
#include <netinet/tcp.h>


int connect_to_tcp_server(const char *ip, unsigned short port)
{
	struct sockaddr_in addr;
	int s;
	int ret;

	ret = inet_aton(ip, &(addr.sin_addr));
	DIE(ret == 0, "Invalid IP address supplied");

	s = socket(AF_INET, SOCK_STREAM, 0);
	DIE(s < 0, "socket");

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	ret = connect(s, (struct sockaddr*)&addr, sizeof(addr));
	DIE(ret < 0, "connect");

	return s;
}

int close_tcp_connection(int sockfd)
{
	int ret = shutdown(sockfd, SHUT_RDWR);
	DIE(ret < 0, "shutdown");

	return close(sockfd);
}

int create_listener(unsigned short port, int backlog, int type)
{
	struct sockaddr_in addr;
	int listenfd;
	int ret;
	int enable = 1;

	if (type == TCP) {
		type = SOCK_STREAM;
	} else {
		if (type == UDP) {
			type = SOCK_DGRAM;
		} else {
			return -1;
		}
	}

	listenfd = socket(PF_INET, type, 0);
	DIE(listenfd < 0, "socket");

	if (type == SOCK_STREAM) { // disable Nagle algorithm for TCP
		ret = disableNagle(listenfd);
		DIE(ret < 0, "disableNagle");
	}

	ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
	DIE(ret < 0, "setsockopt");

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;

	ret = bind(listenfd, (struct sockaddr*)&addr, sizeof(addr));
	DIE(ret < 0, "bind");

	if (type == SOCK_STREAM) {
		ret = listen(listenfd, backlog);
		DIE(ret < 0, "listen");
	}

	return listenfd;
}

/* Disables Nagle's Algorithm for a given socket */
int disableNagle(int sockfd)
{
	int enable = 1;
	return setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY,
					  &enable, sizeof(int));
}