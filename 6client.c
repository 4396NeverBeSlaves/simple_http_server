#include<stdio.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<string.h>
#include<stdlib.h>

int main(int argc,char **argv){
    int sfd,cfd,read_num;
    char buf[BUFSIZ];
    struct sockaddr_in6 servsock,csock;
    socklen_t sock_len=sizeof(servsock);
    
    char in_ip_addr[INET6_ADDRSTRLEN];
    cfd=socket(AF_INET6,SOCK_STREAM,0);

    servsock.sin6_family=AF_INET6;
    servsock.sin6_port=ntohs(atoi(argv[2]));
    printf("connecting to %s:%s\n",argv[1],argv[2]);
    inet_pton(AF_INET6,argv[1],&servsock.sin6_addr);
    
    printf("connection state:%d\n",connect(cfd,(struct sockaddr*)&servsock,sock_len));
    while (1)
    {
        read_num=read(cfd,buf,BUFSIZ);
        printf("%s",buf);
        //memset(buf,0,read_num);
        fgets(buf,BUFSIZ,stdin);
        write(cfd,buf,strlen(buf));
    }
    close(cfd);

    return 0;
}