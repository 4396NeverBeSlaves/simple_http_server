#include "wrap.h"
#include "epoll_operation.h"
#include "httphandle.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/mman.h>

void perror_exit(char* s)
{
    perror(s);
    exit(-1);
}

int Socket(int domain, int type, int protocol)
{
    int sockfd;

    if ((sockfd = socket(domain, type, protocol)) < 0)
        perror_exit("Socket error");
    return sockfd;
}

int Bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen)
{
    if (bind(sockfd, addr, addrlen) < 0)
        perror_exit("Bind error");
    return 0;
}

int Listen(int sockfd, int backlog)
{
    if (listen(sockfd, backlog) < 0)
        perror_exit("Listen error");
    return 0;
}

int Accept(int lfd, struct sockaddr* addr, socklen_t* addrlen)
{
    int cfd;

begin:
    if ((cfd= accept(lfd, (struct sockaddr*)addr, addrlen)) < 0) {
        if (errno == EINTR)
            goto begin;
        else
            perror_exit("Accept error");
    }

    return cfd;
}

int Close(int fd)
{
    if (close(fd) < 0)
        perror_exit("Close error");
    return 0;
}

ssize_t Read(int fd, void* buf, size_t count)
{
    ssize_t rc;

begin:
    if ((rc = read(fd, buf, count)) < 0) {
        if (errno == EINTR)
            goto begin;
        else
            perror_exit("Read error");
    }
    return rc;
}

ssize_t Write(int fd, const void* buf, size_t count)
{
    ssize_t wc;

begin:
    if ((wc = write(fd, buf, count)) < 0) {
        if (errno == EINTR)
            goto begin;
        else
            perror_exit("Write error");
    }
    return wc;
}
ssize_t readn(int fd, void* buf, size_t count)
{
    int has_read = 0;
    size_t left = count;
    char* ptr = buf;

    while (left > 0) {
        if ((has_read = Read(fd, ptr, count)) == 0)
            break;
        ptr = ptr + has_read;
        left = left - has_read;
    }

    return count - left;
}

ssize_t writen(int fd, const void* buf, size_t count)
{
    int left = count;
    size_t has_written;
    char* ptr = (char*)buf;

    while (left > 0) {
        has_written = Write(fd, ptr, left);
        left = left - has_written;
        ptr = ptr + has_written;
    }
    return count;
}

int init_listen_fd(int port)
{
    struct sockaddr_in6 addr;
    int lfd, optval = 1;
    int socklen = sizeof(addr);

    bzero(&addr, socklen);
    addr.sin6_family = AF_INET6;
    addr.sin6_addr = in6addr_any;
    addr.sin6_port = htons(port);

    lfd = Socket(AF_INET6, SOCK_STREAM, 0);
    if (setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int)) < 0)
        return -1;
    Bind(lfd, (struct sockaddr*)&addr, socklen);
    Listen(lfd, 1024);

    return lfd;
}

void *Mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset){
    char *ptr;
    if((ptr=mmap(addr,length,prot,flags,fd,offset))==((void *) -1))
        perror_exit("Mmap error");
    return ptr;
}

int Munmap(void *addr, size_t length){
    if(munmap(addr,length)<0)
        perror_exit("Munmap error");
    return 0;
}
