#include "doubleLinkedList.h"
#include "treap.h"
#include "common.h"
#include <netinet/in.h>
#include <ctype.h>
#define MAX_CONTENT_LEN 1501

typedef struct Topic {
	char *topicName;
	Treap *subs;
	Treap *tempSubs;
} Topic;

typedef struct Packet {
	char *data;
	ssize_t len;
} Packet;

typedef struct Message {
	Packet packet;
	TDoubleLinkedList *subs;
	TDoubleLinkedList *tempSubs;
} Message;

typedef struct hdr {
	uint32_t ip;
	uint16_t port;
	uint8_t type;
} __attribute__((__packed__)) hdr;

Message* create_message(struct sockaddr_in *claddr, char *topicName,
						char *content, uint8_t type, uint16_t size);

void copySubs(Treap *root, TDoubleLinkedList *list);
void copyTempSubs(Treap *root, TDoubleLinkedList *list);
void delete_message(Message *m);
int8_t check_ascii(char *s);

struct Int {
	uint8_t sign;
	uint32_t number;
} __attribute__((__packed__));

struct Short_Real {
	uint16_t number;
} __attribute__((__packed__));

struct Float {
	uint8_t sign;
	uint32_t number;
	uint8_t pow;
} __attribute__((__packed__));

struct String {
	uint16_t len;
} __attribute__((__packed__));

struct udp_hdr {
	char topic[MAX_TOPIC];
	uint8_t type;
} __attribute__((__packed__));
