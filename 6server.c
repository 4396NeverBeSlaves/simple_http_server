#include<stdio.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<string.h>
#include<ctype.h>

#define SERVER_PORT 8000

int main(){
    int sfd,cfd,read_num,i;
    char buf[BUFSIZ],welcome[1024];
    struct sockaddr_in6 ssock,csock;
    socklen_t client_sock_len=sizeof(csock);
    
    char in_ip_addr[100];
    unsigned int in_ip_port;
    
    ssock.sin6_family=AF_INET6;
    ssock.sin6_port=htons(SERVER_PORT);
    ssock.sin6_addr=in6addr_any;
    sfd=socket(AF_INET6,SOCK_STREAM,0);
    printf("serv_fd:%d bind state:%d\n",sfd,bind(sfd,(struct sockaddr *)&ssock,sizeof(struct sockaddr_in6)));
    if(listen(sfd,128)==0)
        printf("listen success\n");
    else
    {
        printf("error!\n");
    }
    
    cfd=accept(sfd,(struct sockaddr *)&csock,&client_sock_len);
    printf("client fd:%d\n",cfd);
    inet_ntop(AF_INET6,(void *)&csock.sin6_addr,in_ip_addr,INET6_ADDRSTRLEN);
    in_ip_port=ntohs(csock.sin6_port);
    printf("connection from %s:%d\n",in_ip_addr,in_ip_port);
    sprintf(welcome,"hello, %s:%d\n",in_ip_addr,in_ip_port);
    write(cfd,welcome,strlen(welcome));
    while(1){
        read_num=read(cfd,buf,BUFSIZ);
        printf("read_num:%d,%s",read_num,buf);
        for(i=0;i<read_num;i++){
            buf[i]=toupper(buf[i]);
        }
        write(cfd,buf,read_num);
        memset(buf,0,read_num);
    }
    close(cfd);
    close(sfd);
    return 0;
}