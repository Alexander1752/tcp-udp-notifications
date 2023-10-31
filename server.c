#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "util.h"
#include "sock_util.h"
#include "common.h"
#include "data_util.h"
#include "doubleLinkedList.h"
#include "treap.h"
#include <ctype.h>		 // for isdigit()
#include <time.h> 		 // for rand() for treap
#include <netinet/tcp.h> // for TCP_NODELAY
#include <signal.h>		 // for disabling SIGPIPE

const uint16_t dataSizes[] = {
		sizeof(struct Int),
		sizeof(struct Short_Real),
		sizeof(struct Float),
		sizeof(struct String)
};

/* server sockets file descriptors */
static int tcpfd;
static int udpfd;

/* epoll file descriptor */
static int epollfd;

static struct Client *clients = NULL;
static int noClients = 0;
Treap *clientTreap = NULL;

Treap *topics = NULL;

TDoubleLinkedList *messages = NULL;
int messagesToSend = 0;

Topic* create_topic(char *topicName)
{
	Topic *topic = (Topic*)malloc(sizeof(Topic));
	DIE(topic == NULL, "malloc");
	topic->topicName = strdup(topicName);
	DIE(topic->topicName == NULL, "strdup");

	topic->subs = topic->tempSubs = NULL;

	insert_key(&topics, topic);
	return topic;
}

/*
 * Initializes client structure with given socket.
 */
void init_client(struct Client *client, int sockfd)
{
	client->sockfd = sockfd;
	client->recv_len = 0;
	client->send_len = 0;
	client->sent = 0;
	client->packet_begun = 0;
	client->prior_dle = 0;
	client->state = IDLE;
}

static struct Client *create_client(char* id)
{
	static int maxClients = START_SIZE;
	struct Client *client;

	if (clients == NULL) {
		clients = (struct Client*)malloc(maxClients * sizeof(struct Client));
		DIE(clients == NULL, "malloc");
	}

	if (noClients == maxClients) {
		maxClients *= 2;
		clients = (struct Client*)realloc(clients, maxClients * sizeof(struct Client));
		DIE(clients == NULL, "realloc");
	}
	client = &(clients[noClients++]);
	client->id = strdup(id);
	DIE(client->id == NULL, "strdup");

	insert_key(&clientTreap, client);

	return client;
}

void disconnect_client(struct Client *client)
{
	if (client->state == CONNECTION_CLOSED)
		return;

	int ret = close_tcp_connection(client->sockfd);
	DIE(ret < 0, "close");

	client->state = CONNECTION_CLOSED;
	client->recv_len = client->packet_begun = 0;
	client->send_len = client->sent = 0;

	printf("Client %s disconnected.\n", client->id);

	/* remove client from messages that he's tempSubscribed to */
	node *m = messages->sentinel->next;
	while (m != messages->sentinel) {
		Message *message = (Message*)(m->data);
		node *sentinel = message->tempSubs->sentinel;
		node *c = sentinel->next;
		while (c != sentinel) {
			if ((struct Client*)(c->data) == client) {
				removeNode(message->tempSubs, c);
				break;
			}
			c = c->next;
		}
		m = m->next;
	}
}

/*
 * Handles a new TCP connection request on the server socket.
 */
static void handle_tcp_connection(void)
{
	static int sockfd;
	socklen_t addrlen = sizeof(struct sockaddr_in);
	struct sockaddr_in addr;
	struct Client *client;
	int ret, enable = 1;

	/* accept new connection */
	sockfd = accept(tcpfd, (struct sockaddr*)&addr, &addrlen);
	DIE(sockfd < 0, "accept");

	ret = fcntl(sockfd, F_SETFL, O_NONBLOCK); // non-blocking socket
	DIE(ret < 0, "fcntl");

	ret = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY,
					&enable, sizeof(int));
	DIE(ret < 0, "setsockopt");

	int8_t bytes_recv = 0, len = sizeof(struct conn_hdr);
	struct conn_hdr conn;
	char *buf = (char*)&conn;

	while (bytes_recv < len) {
		ret = recv(sockfd, buf + bytes_recv, len - bytes_recv, MSG_DONTWAIT);
		if (ret < 0) {
			DIE(errno != EAGAIN && errno != EWOULDBLOCK, "recv");
			continue;
		}

		if (ret == 0)
			goto disconnect;

		bytes_recv += ret;
	}

	if (conn.start_delim[0] != DLE || conn.end_delim[0] != DLE ||
		conn.start_delim[1] != STX || conn.end_delim[1] != ETX)
		goto disconnect;

	conn.end_delim[0] = 0;
	if (!check_ascii(conn.id))
		goto disconnect;

	char *id = conn.id;

	Treap *n = search(clientTreap, &(id));

	if (n) {
		client = (struct Client*)(n->key);
		if (client->state != CONNECTION_CLOSED) {
			printf("Client %s already connected.\n", client->id);
			goto disconnect;
		}
	}
	else
		client = create_client(conn.id);

	/* initialise client fields with default values */
	init_client(client, sockfd);

	/* add socket to epoll */
	ret = epoll_add_in(epollfd, sockfd, client);
	DIE(ret < 0, "epoll_add_in");


	printf("New client %s connected from: %s:%d.\n", client->id,
		   inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

	return;

disconnect:
	ret = close_tcp_connection(sockfd);
	DIE(ret < 0, "close");
	return;
}


/* 
 * Handles a new UDP connection from a client and
 * processes the message received to be sent to TCP clients.
 */
static void handle_udp_connection(void)
{
	struct sockaddr_in claddr;
	socklen_t len = sizeof(claddr);

	char buf[MAX_UDP + 1];
	memset(buf, 0, MAX_UDP + 1);

	struct udp_hdr *header = (struct udp_hdr*)buf;

	uint16_t bytes_recv;

	bytes_recv = recvfrom(udpfd, buf, MAX_UDP, MSG_WAITALL,
						  (struct sockaddr *)&claddr, &len);
	DIE(bytes_recv < 0, "recvfrom");

	uint8_t type;
	char *topicName, *content;

	type = header->type;

	header->type = 0; // add '\0' in case topic name doesn't already have one
	topicName = header->topic;

	if (!check_ascii(topicName))
		goto invalid_udp_packet;

	content = buf + sizeof(struct udp_hdr);

	uint16_t contentLen;
	switch (type) {
		case STRING:
			contentLen = (uint16_t)(strlen(content) + 1);
			break;
		case INT:
		case SHORT_REAL:
		case FLOAT:
			contentLen = dataSizes[type];
			break;
		default:
			goto invalid_udp_packet;
	}

	type = ((uint8_t)(strlen(topicName) + 1) << 2) + type;

	Treap *n = search(topics, &topicName);
	Topic *topic;

	if (n == NULL)
		topic = create_topic(topicName);
	else
		topic = (Topic*)(n->key);

	Message *m = create_message(&claddr, topicName, content, type, contentLen);

	copySubs(topic->subs, m->subs);
	copyTempSubs(topic->tempSubs, m->tempSubs);

	if (empty(m->subs) && empty(m->tempSubs)) {
		delete_message(m);
	} else {
		addLast(messages, m);
		messagesToSend++;
	}

	return;

invalid_udp_packet:
	fprintf(stderr, "Invalid packet received on UDP\n");
	return;
}


/* Checks the integrity of a(n) (un)subscribe packet */
void check_packet(struct Client* client) {
	uint8_t dataType = *(uint8_t*)(client->recv_buf);

	client->recv_len--;

	if (client->recv_len != (dataType >> 2) ||
		client->recv_buf[client->recv_len] != 0)
		goto invalid_subscribe_packet;

	char *topicName = client->recv_buf + 1;

	if (!check_ascii(topicName))
		goto invalid_subscribe_packet;

	Topic *topic;
	Treap *n = search(topics, &topicName);

	if (n == NULL)
		topic = create_topic(topicName);
	else topic = n->key;

	// if already subscribed in some way to this topic, unsubscribe
	delete_key(&(topic->subs), client);
	delete_key(&(topic->tempSubs), client);

	if (dataType & SUBSCRIBE) { // subscribe request
		switch (dataType & STORE_N_FORWARD) {
			case 0: // tempSubscriber
				insert_key(&(topic->tempSubs), client);
				break;
			default: // SF subscriber
				insert_key(&(topic->subs), client);
				break;
		}
	}

invalid_subscribe_packet:
	fprintf(stderr, "Malformed client packet received\n");
	return;
}

static void queue_to_send(struct Client *client, Message *m) {
	client->sent = 0;
	client->send_len = m->packet.len;

	memcpy(client->send_buf, m->packet.data, client->send_len);

	int ret = epoll_set_inout(epollfd, client->sockfd, client);
	DIE(ret < 0, "epoll_set_inout");
}

static void get_next_in_queue()
{
	if (messagesToSend == 0)
		return;

	node *m = messages->sentinel->next;
	while (m != messages->sentinel) {
		Message *message = (Message*)(m->data);
		node *sentinel, *c;

		if (message->tempSubs != NULL) {
			sentinel = message->tempSubs->sentinel;
			c = sentinel->next;
			while (c != sentinel) {
				struct Client *client = (struct Client*)(c->data);

				if (client->state == CONNECTION_CLOSED)
					c = removeNode(message->tempSubs, c);
				else if (client->state == IDLE) {
					queue_to_send(client, message);
					c = removeNode(message->tempSubs, c);

					/* erase message if sent to all recipients */
					if (empty(message->tempSubs)) {
						free_list(&(message->tempSubs));
						if (empty(message->subs)) {
							/* both subs list are empty */
							delete_message(message);
							removeNode(messages, m);
							messagesToSend--;
							return;
						}
						break;
					}
				}
				c = c->next;
			}
		}

		if (message->subs != NULL) {
			sentinel = message->subs->sentinel;
			c = sentinel->next;
			while (c != sentinel) {
				struct Client *client = (struct Client*)(c->data);

				if (client->state == IDLE) {
					queue_to_send(client, message);
					c = removeNode(message->subs, c);

					/* erase message if sent to all recipients */
					if (empty(message->subs)) {
						free_list(&(message->subs));
						if (empty(message->tempSubs)) {
							/* both subs list are empty */
							delete_message(message);
							removeNode(messages, m);
							messagesToSend--;
							return;
						}
						break;
					}
				}
				c = c->next;
			}
		}
		m = m->next;
	}
}

int main(int argc, char* argv[])
{
	int ret;

	ret = setvbuf(stdout, NULL, _IONBF, 0);
	DIE(ret != 0, "setvbuf");

	if (argc != 2) {
		printf(" Usage: %s <port>\n", argv[0]);
		return 1;
	}

	void *sig = signal(SIGPIPE, SIG_IGN); // disable SIGPIPE
	DIE(sig == SIG_ERR, "signal");

	char *s = argv[1];
	while (isdigit(*s++));
	if (*(s - 1) != 0)
		goto invalid_port;

	int port;
	sscanf(argv[1], "%d", &port);
	if (port > UINT16_MAX)
		goto invalid_port;

	srand(time(NULL));

	/* init epoll */
	epollfd = epoll_create(START_SIZE);
	DIE(epollfd < 0, "epoll_create");

	/* create listening sockets */
	tcpfd = create_listener(port, DEFAULT_LISTEN_BACKLOG, TCP);
	DIE(tcpfd < 0, "create_listener_tcp");
	udpfd = create_listener(port, DEFAULT_LISTEN_BACKLOG, UDP);
	DIE(udpfd < 0, "create_listener_udp");

	ret = epoll_add_fd_in(epollfd, tcpfd);
	DIE(ret < 0, "epoll_add_in");
	ret = epoll_add_fd_in(epollfd, udpfd);
	DIE(ret < 0, "epoll_add_in");
	ret = epoll_add_fd_in(epollfd, 0); // stdin
	DIE(ret < 0, "epoll_add_in");

	init(&messages);

	/* server main loop */
	while (1) {
		struct epoll_event rev;

		ret = epoll_wait(epollfd, &rev, 1, MAX_WAIT_TIME);
		DIE(ret < 0, "epoll_wait");

		/*
		 * switch event types; consider
		 *   - new connection requests (on server sockets)
		 *   - stdin commands from user
		 *   - socket communication (on client sockets)
		 */

		if (ret != 0) {
			if (rev.data.fd == tcpfd) {
				if (rev.events & EPOLLIN)
					handle_tcp_connection();
			} else if (rev.data.fd == udpfd) {
				if (rev.events & EPOLLIN)
					handle_udp_connection();
			} else if (rev.data.fd == 0) { // stdin
				char *command = recv_stdin();
				if (command != NULL && strcmp(command, "exit") == 0) {
					for (int i = 0; i < noClients; ++i)
						disconnect_client(clients + i);

					free(clients);

					ret = close(epollfd);
					DIE(ret < 0, "close");

					ret = close_tcp_connection(tcpfd);
					DIE(ret < 0, "close");
					
					ret = close(udpfd);
					DIE(ret < 0, "close");

					return 0;
				}
			} else {
				if (rev.events & EPOLLIN) {
					receive_packet(rev.data.ptr, epollfd);
				} else if (rev.events & EPOLLOUT) {
					send_packet(rev.data.ptr, epollfd);
				}
			}
		} else {
			get_next_in_queue();
		}

	}

	return 0;

invalid_port:
	printf(" %s is not a valid port\n", argv[1]);
	return 1;
}
