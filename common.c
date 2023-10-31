#include "common.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

/* Checks whether given string contains only printable ASCII characters */
int8_t check_ascii(char *s)
{
	while (isprint(*s++));
	return *(s - 1) == 0;
}

/*
 * Sends the packet that was queued in struct Client on the socket
 * contained in said struct.
 */
enum connection_state send_packet(struct Client *client, int epollfd)
{
	ssize_t bytes_sent;
	int ret;

	if (client->sent <= client->send_len) { // sending header
		bytes_sent = send(client->sockfd, client->send_buf + client->sent,
							client->send_len - client->sent, 0);
		if (bytes_sent < 0) {
			if (errno != EAGAIN && errno != EWOULDBLOCK) // error in communication
				goto remove_client;
			else return SENDING_DATA;
		}

		client->sent += bytes_sent;
		if (client->sent >= client->send_len) {
			ret = epoll_set_in(epollfd, client->sockfd, client);
			DIE(ret < 0, "epoll_set_in");

			client->state = IDLE;
			return IDLE;
		}
	}

	return SENDING_DATA;

remove_client:
	/* remove current connection */
	disconnect_client(client);

	return CONNECTION_CLOSED;
}


/*
 * Receives (possibly multiple) packets on socket specified in struct Client.
 * Stores request(s) in recv_buffer in struct Client.
 */
enum connection_state receive_packet(struct Client *client, int epollfd)
{
	ssize_t bytes_recv;
	int ret;

	char temp[BUFSIZ + 1], *first = temp + 1;
	temp[0] = DLE;

	bytes_recv = recv(client->sockfd, first, BUFSIZ, 0);
	if (bytes_recv == 0) // client closed
		goto remove_client;

	if (bytes_recv < 0) {
		if (errno != EAGAIN && errno != EWOULDBLOCK)
			goto remove_client;
		else return client->state;
	}

	client->state = RECEIVING_DATA;

	first[bytes_recv] = 0; // add 0-byte to end of data

	char *s = first;

	if (client->prior_dle) {
		s--;
		client->prior_dle = 0;
	}

	if (client->packet_begun)
		goto check_end;
	
	while (1) {
check_begin:
		while (s < first + bytes_recv) {
			if (*s++ == DLE) {
				if (*s == STX) {
					/* packet begins here */
					client->packet_begun = 1;
					client->recv_len = 0;
					s++;
					goto check_end;
				} else {
					if (s < first + bytes_recv)
						s++;
					else client->prior_dle = 1;
				}
			}
		}
		break;

check_end:
		while (s < first + bytes_recv) {
			if (*s == DLE) {
				if (*(++s) == ETX) {
					/* packet ends here */
					client->packet_begun = 0;
					check_packet(client);
					client->recv_len = 0;
					goto check_begin;
				} else if (*s == STX) {
					/* previous packet was corrupt; a new one begins */
					client->packet_begun = 1;
					client->recv_len = 0;
					s++;
					continue;
				}
			}
			if (s < first + bytes_recv)
				client->recv_buf[client->recv_len++] = *s++;
			else client->prior_dle = 1;
		}
		break;
	}

	if (client->packet_begun == 0) {
		
		client->state = IDLE;

		ret = epoll_set_in(epollfd, client->sockfd, client);
		DIE(ret < 0, "epoll_set_in");

		return IDLE;
	}

	return RECEIVING_DATA;

remove_client:
	/* remove current connection */
	disconnect_client(client);

	return CONNECTION_CLOSED;
}

/*
 * Reads from stdin until newline and keeps the result in its
 * static buffer until a subsequent call.
 * Returns the address of the buffer if a valid line was read
 * or NULL otherwise.
 */
char* recv_stdin()
{
	static char buf[BUFSIZ];
	static int size = 0;
	static char *pos = NULL;
	int ret;

	if (pos) {
		size -= pos + 1 - buf;
		memmove(buf, pos + 1, size);
		pos = NULL;
	}

	ret = read(0, buf + size, BUFSIZ - size - 1);
	DIE(ret < 0, "read stdin");

	size += ret;
	buf[size] = 0;

	pos = strchr(buf, '\n');
	if (pos != NULL) {
		pos[0] = 0;
		if (check_ascii(buf))
			return buf;
	}

	return NULL;
}