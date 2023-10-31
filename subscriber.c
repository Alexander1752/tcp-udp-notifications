#include <stdlib.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "data_util.h"
#include "sock_util.h"
#include "doubleLinkedList.h"
#include "epoll_util.h"

struct Client cl, *client;
int epollfd;

TDoubleLinkedList *requests = NULL;

/* Checks the integrity of a message */
void check_packet(struct Client* client) {
	hdr *header = (hdr*)client->recv_buf;
	uint8_t topicLen = header->type >> 2;

	char *topic = client->recv_buf + sizeof(hdr);
	if (!check_ascii(topic) || strlen(topic) != topicLen - 1)
		goto invalid_message;

	struct in_addr addr;
	addr.s_addr = header->ip;
	char *ip = inet_ntoa(addr);

	struct String *content4;

	switch (header->type & TYPE) {
		case INT:
			if (client->recv_len !=
				sizeof(hdr) + topicLen + sizeof(struct Int))
				goto invalid_message;

			struct Int *content = (struct Int*)
				(client->recv_buf + sizeof(hdr) + topicLen);

			if (content->sign > 1) // different from 0 and 1
				goto invalid_message;

			printf("%s:%hu - %s - INT - %s%u\n", ip, ntohs(header->port),
				   topic, content->sign ? "-" : "", ntohl(content->number));

			return;

		case SHORT_REAL:
			if (client->recv_len !=
				sizeof(hdr) + topicLen + sizeof(struct Short_Real))
				goto invalid_message;

			struct Short_Real *content2 =
				(struct Short_Real*)(client->recv_buf + sizeof(hdr) + topicLen);

			content2->number = ntohs(content2->number);

			printf("%s:%hu - %s - SHORT_REAL - %.2lf\n", ip,
				   ntohs(header->port), topic, content2->number / 100.0);

			return;

		case FLOAT:
			if (client->recv_len !=
				sizeof(hdr) + topicLen + sizeof(struct Float))
				goto invalid_message;

			struct Float *content3 =
				(struct Float*)(client->recv_buf + sizeof(hdr) + topicLen);

			if (content3->sign > 1) // different from 0 and 1
				goto invalid_message;

			double pow10 = 1;
			for (int i = 0; i < content3->pow; ++i)
				pow10 *= 10;

			char fmt[1024];
			sprintf(fmt, "%%s:%%hu - %%s - FLOAT - %%s%%.%hhulf\n",
					content3->pow);

			printf(fmt, ip, ntohs(header->port), topic,
				   content3->sign ? "-" : "", ntohl(content3->number) / pow10);

			return;

		case STRING:
			content4 = (struct String*)(client->recv_buf + sizeof(hdr) + topicLen);
			content4->len = ntohs(content4->len);

			if (content4->len > MAX_CONTENT_LEN)
				goto invalid_message;

			if (content4->len !=
				client->recv_len - topicLen - sizeof(hdr) - sizeof(struct String))
				goto invalid_message;

			char *str = (char*)content4 + sizeof(struct String);
			if (!check_ascii(str) || strlen(str) != content4->len - 1)
				goto invalid_message;

			printf("%s:%hu - %s - STRING - %s\n", ip, ntohs(header->port),
				   topic, str);

			return;

		default:
			break;
	}

invalid_message:
	fprintf(stderr, "Malformed message received from server");
	return;
}

void disconnect_client(struct Client *client)
{
	int ret;

	ret = close_tcp_connection(client->sockfd);
	DIE(ret < 0, "close");

	client->state = CONNECTION_CLOSED;
	client->recv_len = client->packet_begun = 0;
	client->send_len = client->sent = 0;

	ret = epoll_remove(epollfd, 0); // stdin
	DIE(ret < 0, "epoll_remove");

	ret = close(epollfd);
	DIE(ret < 0, "close");

	exit(0);
}

Packet* create_request(uint8_t type, char *topicName)
{
	char buf[BUFSIZ];
	char *s = buf;
	*s++ = DLE;
	*s++ = STX;
	uint8_t len = strlen(topicName);
	*s++ = ((len + 1) << 2) | type;
	if (*(s - 1) == DLE)
		*s++ = DLE;

	for (ssize_t i = 0; i <= len; ++i)
		*s++ = *topicName++;

	*s++ = DLE;
	*s++ = ETX;

	Packet *pack = malloc(sizeof(Packet));
	DIE(pack == NULL, "malloc");
	pack->len = s - buf;
	pack->data = malloc(pack->len * sizeof(char));
	DIE(pack->data == NULL, "malloc");

	memcpy(pack->data, buf, pack->len);

	return pack;
}

static void queue_to_send(struct Client *client, Packet *pack) {
	client->sent = 0;
	client->send_len = pack->len;
	client->state = SENDING_DATA;

	memcpy(client->send_buf, pack->data, client->send_len);
	epoll_set_inout(epollfd, client->sockfd, client);

	if (pack->data[2] & SUBSCRIBE)
		printf("Subscribed to topic.\n");
	else printf("Unsubscribed from topic.\n");

	free(pack->data);
	free(pack);
}

int main(int argc, char* argv[])
{
	int ret;

	ret = setvbuf(stdout, NULL, _IONBF, 0);
	DIE(ret != 0, "setvbuf");

	if (argc != 4) {
		printf("\n Usage: %s <ID> <IP> <PORT>\n", argv[0]);
		return 1;
	}

	char *s = argv[3];
	while (isdigit(*s++));
	if (*(s - 1) != 0)
		goto invalid_port;

	unsigned int port;
	sscanf(argv[3], "%u", &port);
	if (port > UINT16_MAX)
		goto invalid_port;

	if (!check_ascii(argv[1]) || strlen(argv[1]) > MAX_ID_LEN) {
		printf(" %s is not a valid ID\n", argv[1]);
		return 1;
	}

	/* init epoll */
	epollfd = epoll_create(START_SIZE);
	DIE(epollfd < 0, "epoll_create");

	/* init socket */
	int sockfd = connect_to_tcp_server(argv[2], port);
	ret = disableNagle(sockfd);
	DIE(ret < 0, "disableNagle");

	/* init client structure */
	client = &cl;
	memset(client, 0, sizeof(struct Client));
	client->id = argv[1];
	client->sockfd = sockfd;

	struct conn_hdr *conn = (struct conn_hdr*)client->send_buf;
	conn->start_delim[0] = DLE;
	conn->end_delim[0] = DLE;
	conn->start_delim[1] = STX;
	conn->end_delim[1] = ETX;
	strncpy(conn->id, client->id, MAX_ID_LEN);

	client->state = SENDING_DATA;
	client->send_len = sizeof(struct conn_hdr);

	ret = epoll_add_inout(epollfd, sockfd, client);
	DIE(ret < 0, "epoll_add_out");
	ret = epoll_add_fd_in(epollfd, 0); // stdin
	DIE(ret < 0, "epoll_add_in");

	uint8_t SF;

	init(&requests);

	/* client main loop */
	while (1) {
		struct epoll_event rev;

		ret = epoll_wait(epollfd, &rev, 1, MAX_WAIT_TIME);
		DIE(ret < 0, "epoll_wait");

		/*
		 * switch event types; consider
		 *   - server socket communication (send/receive)
		 *   - stdin commands from user
		 */

		if (ret != 0) {
			if (rev.data.ptr == client) {
				if (rev.events & EPOLLIN)
					receive_packet(client, epollfd);
				else if (rev.events & EPOLLOUT)
					send_packet(client, epollfd);
			} else if (rev.data.fd == 0) { // stdin
				char *command = recv_stdin();
				if (command != NULL) {
					char sub[13], topicName[MAX_TOPIC + 1];
					SF = 0xFF;
					sscanf(command, "%12s%50s%1hhu", sub, topicName, &SF);
					if (strcmp(sub, "subscribe") == 0) {
						if (SF > 1) // invalid SF value
							continue;
						if (!check_ascii(topicName)) // invalid topic
							continue;

						SF = (SF << 1) | SUBSCRIBE; // subscribe

						addLast(requests, create_request(SF, topicName));
					} else if (strcmp(sub, "unsubscribe") == 0) {
						if (!check_ascii(topicName)) // invalid topic
							continue;

						SF = 0; // unsubscribe

						addLast(requests, create_request(SF, topicName));
					} else if (strcmp(sub, "exit") == 0)
						disconnect_client(client);
				}
			}
		} else if (client->state == IDLE) { // ready to send 
			if (!empty(requests)) {
				node *n = removeNodeAt(requests, 0);
				queue_to_send(client, n->data);
				free(n);
			}
		}
	}

	return 0;

invalid_port:
	printf(" %s is not a valid port\n", argv[3]);
	return 1;
}
