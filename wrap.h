#ifndef __WRAP_H__
#define __WRAP_H__
#include <arpa/inet.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

void perror_exit(char* s);
void get_process_path(char* path);
int Socket(int domain, int type, int protocol);
int Bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
int Listen(int sockfd, int backlog);
int Accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen);
int Connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
int Close(int fd);
ssize_t Read(int fd, void* buf, size_t count);
ssize_t Write(int fd, const void* buf, size_t count);
ssize_t readn(int fd, void* buf, size_t count);
ssize_t writen(int fd, const void* buf, size_t count);

int init_listen_fd(int port);

void* Mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset);
int Munmap(void* addr, size_t length);
#endif
