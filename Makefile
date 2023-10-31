CFLAGS = -Wall -g

.PHONY: all clean

build: server subscriber

all: server subscriber

server: server.o sock_util.o epoll_util.o data_util.o treap.o doubleLinkedList.o common.o

server.o: server.c sock_util.h util.h

subscriber: subscriber.o common.o data_util.o sock_util.o doubleLinkedList.o epoll_util.o

subscriber.o: subscriber.c common.h data_util.h sock_util.h doubleLinkedList.h

sock_util.o: sock_util.c sock_util.h util.h

common.o: common.c common.h util.h

epoll_util.o: epoll_util.c epoll_util.h

data_util.o: data_util.c data_util.h

treap.o: treap.c treap.h

doubleLinkedList.o: doubleLinkedList.c doubleLinkedList.h

clean:
	-rm -f *.o
	-rm -f server
	-rm -f subscriber
