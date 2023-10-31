#include <stdio.h>
#include <errno.h>
#include "util.h"
#include "epoll_util.h"
#ifndef COMMON_H

	#define COMMON_H
	#define START_SIZE 10
	#define MAX_UDP 1551
	#define MAX_TOPIC 50
	#define MAX_ID_LEN 10
	#define MAX_WAIT_TIME 0

	#define STX '\2'
	#define ETX '\3'
	#define DLE '\20'

	#define TYPE 0b11
	#define SUBSCRIBE 1
	#define STORE_N_FORWARD 0b10


	enum Protocol {TCP, UDP};

	enum connection_state {
		IDLE,
		CONNECTION_CLOSED,
		RECEIVING_DATA,
		SENDING_DATA
	};

	enum data_type {INT, SHORT_REAL, FLOAT, STRING};

	struct Client {
		char *id;
		int sockfd;

		char recv_buf[BUFSIZ];
		__uint16_t recv_len;
		__int8_t packet_begun;
		__int8_t prior_dle;

		char send_buf[BUFSIZ];
		__uint16_t send_len;
		size_t sent;

		enum connection_state state;
	};

	struct conn_hdr {
		char start_delim[2];
		char id[MAX_ID_LEN];
		char end_delim[2];
	} __attribute__((__packed__));

	enum connection_state send_packet(struct Client *client, int epollfd);
	enum connection_state receive_packet(struct Client *client, int epollfd);
	extern void disconnect_client(struct Client *client);
	extern void check_packet(struct Client* client);
	int8_t check_ascii(char *s);
	char* recv_stdin();
#endif
