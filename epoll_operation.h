#ifndef __EPOLL_OPERATION_H__
#define __EPOLL_OPERATION_H__

#include <sys/epoll.h>
#include "wrap.h"

#define MAXEVENTS 1000

int set_nonblocking(int fd);
int Epoll_create(int size);
int Epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
int Epoll_wait(int epfd, struct epoll_event *events,int maxevents, int timeout);
int addfd(int epfd,int fd);
int modfd(int epfd,int fd,int operation);
int delfd(int epfd,int fd);
#endif