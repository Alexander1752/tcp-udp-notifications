#include <string.h>
#include <stdlib.h>
#include "data_util.h"
#include <sys/socket.h>
#include "util.h"

/* 
 * Compares two structures that contain at the beginning 
 * a pointer to a string.
 */
int cmp(void *d1, void *d2)
{
    return strcmp(*(char**)d1, *(char**)d2);
}

/* Creates a packet to be sent to the client */
Packet create_packet(struct sockaddr_in *claddr, char *topicName,
					 char *content, uint8_t type, uint16_t size)
{
	hdr header;
	header.ip = claddr->sin_addr.s_addr;
	header.port = claddr->sin_port;
	header.type = type;

	char buf[BUFSIZ], *s = buf, *h = (char*)(&header);
	*s++ = DLE;
	*s++ = STX;

	/* copy header of packet */
	for (ssize_t i = 0; i < sizeof(header); ++i) {
		if (*h == DLE)
			*s++ = DLE;
		
		*s++ = *h++;
	}

	/* copy topic name */
	do {
		if (*topicName == DLE)
			*s++ = DLE;
		
		*s++ = *topicName;
	} while (*topicName++ != 0);

	/* if content is STRING, copy the length of the string beforehand*/
	if ((type & TYPE) == STRING) {
		struct String str_hdr;
		str_hdr.len = htons(size); // length in network order
		char *ptr = (char*)&str_hdr;
		for (ssize_t i = 0; i < sizeof(str_hdr); ++i) {
			if (ptr[i] == DLE)
				*s++ = DLE;
			
			*s++ = ptr[i];
		}
	}

	/* copy actual content */
	for (ssize_t i = 0; i < size; ++i) {
		if (*content == DLE)
			*s++ = DLE;
		
		*s++ = *content++;
	}

	*s++ = DLE;
	*s++ = ETX;

	Packet pack;
	pack.len = s - buf;
	pack.data = (char*)malloc(pack.len);
	DIE(pack.data == NULL, "malloc");

	memcpy(pack.data, buf, pack.len);
	return pack;
}

/* Creates a message to be queued for subscribers */
Message* create_message(struct sockaddr_in *claddr, char *topicName,
						char *content, uint8_t type, uint16_t size)
{
	Message *m = (Message*)malloc(sizeof(Message));
	DIE(m == NULL, "malloc");
	m->packet = create_packet(claddr, topicName, content, type, size);
	init(&(m->subs));
	init(&(m->tempSubs));

	return m;
}

void delete_message(Message *m)
{
	free(m->packet.data);
	free_list(&(m->subs));
	free_list(&(m->tempSubs));
	free(m);
}

/* Copies all subscribers to a topic with SF in a list */
void copySubs(Treap *root, TDoubleLinkedList *list)
{
    if(root==NULL)
        return;
    copySubs(root->left, list);
    addLast(list, root->key);
    copySubs(root->right, list);
}

/*
 * Copies all subscribers to a topic without SF
 * that are currently connected in a list
 */
void copyTempSubs(Treap *root, TDoubleLinkedList *list)
{
    if(root==NULL)
        return;
    copyTempSubs(root->left, list);
	if (((struct Client*)(root->key))->state != CONNECTION_CLOSED)
    	addLast(list, root->key);
    copyTempSubs(root->right, list);
}
