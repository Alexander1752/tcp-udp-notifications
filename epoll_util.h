#ifndef EPOLL_UTIL_H
#define EPOLL_UTIL_H 1

#include <sys/epoll.h>

int epoll_add_in(int epollfd, int fd, void *ptr);
int epoll_add_fd_in(int epollfd, int fd);
int epoll_add_out(int epollfd, int fd, void *ptr);
int epoll_add_inout(int epollfd, int fd, void *ptr);
int epoll_set_in(int epollfd, int fd, void *ptr);
int epoll_set_out(int epollfd, int fd, void *ptr);
int epoll_set_inout(int epollfd, int fd, void *ptr);
int epoll_remove(int epollfd, int fd);
int epoll_wait_infinite(int epollfd, struct epoll_event *rev);

#endif