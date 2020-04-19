#include <sys/epoll.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include "epoll_operation.h"
#include "wrap.h"

int set_nonblocking(int fd){
    int flag;

    if((flag=fcntl(fd,F_GETFL))<0)
        perror_exit("fcntl(fd,F_GETFL) error");
    flag=flag|O_NONBLOCK;
    if((flag=fcntl(fd,F_SETFL,flag))<0)
        perror_exit("fcntl(fd,F_SETFL,flag) error");

    return 0;
}
int Epoll_create(int size){
    int epfd;

    if((epfd=epoll_create(size))<=0)
        perror_exit("Epoll_create error");

    return epfd;
}

int Epoll_ctl(int epfd, int op, int fd, struct epoll_event *event){
    int ret;
    // printf("epfd:%d,op:%d,fd:%d,Event.data.fd:%d\n",epfd,op,fd,event->data.fd);
    if((ret=epoll_ctl(epfd,op,fd,event))<0){
        perror_exit("Epoll_ctl error");
    }

    return ret;
}

int Epoll_wait(int epfd, struct epoll_event *events,int maxevents, int timeout){
    int ret;

begin:
    if((ret=epoll_wait(epfd,events,maxevents,timeout))<0){
        if(errno==EINTR)
            goto begin;
        else
            perror_exit("Epoll_wait error");
    }

    return ret;
}

int addfd(int epfd,int fd){
    struct epoll_event evt;

    set_nonblocking(fd);
    evt.events=EPOLLIN|EPOLLET;
    evt.data.fd=fd;
    printf("epfd:%d cfd:%d\n",epfd,fd);
    Epoll_ctl(epfd,EPOLL_CTL_ADD,fd,&evt);

    return 0;
} 

int modfd(int epfd,int fd,int operation){
    struct epoll_event evt;

    evt.events=operation|EPOLLET;
    evt.data.fd=fd;

    Epoll_ctl(epfd,EPOLL_CTL_MOD,fd,&evt);

    return 0;
}

int delfd(int epfd,int fd){
    struct epoll_event evt;

    evt.data.fd=fd;
    Epoll_ctl(epfd,EPOLL_CTL_DEL,fd,&evt);
    
    return 0;
}