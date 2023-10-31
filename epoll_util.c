#include "epoll_util.h"

int epoll_add_in(int epollfd, int fd, void *data)
{
	struct epoll_event ev;

	ev.events = EPOLLIN;
	ev.data.ptr = data;

	return epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
}

int epoll_add_fd_in(int epollfd, int fd)
{
	struct epoll_event ev;

	ev.events = EPOLLIN;
	ev.data.fd = fd;

	return epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
}

int epoll_add_out(int epollfd, int fd, void *data)
{
	struct epoll_event ev;

	ev.events = EPOLLOUT;
	ev.data.ptr = data;

	return epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
}

int epoll_add_inout(int epollfd, int fd, void *data)
{
	struct epoll_event ev;

	ev.events = EPOLLIN | EPOLLOUT;
	ev.data.ptr = data;

	return epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
}

int epoll_set_in(int epollfd, int fd, void *data)
{
	struct epoll_event ev;

	ev.events = EPOLLIN;
	ev.data.ptr = data;

	return epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &ev);
}

int epoll_set_out(int epollfd, int fd, void *data)
{
	struct epoll_event ev;

	ev.events = EPOLLOUT;
	ev.data.ptr = data;

	return epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &ev);
}

int epoll_set_inout(int epollfd, int fd, void *data)
{
	struct epoll_event ev;

	ev.events = EPOLLIN | EPOLLOUT;
	ev.data.ptr = data;

	return epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &ev);
}

int epoll_remove(int epollfd, int fd)
{
	struct epoll_event ev;
	return epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &ev);
}

int epoll_wait_infinite(int epollfd, struct epoll_event *rev)
{
	return epoll_wait(epollfd, rev, 1, -1);
}
