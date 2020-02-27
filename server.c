#include<stdio.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<string.h>

#define SERVER_PORT 8000

int main(){
    int sfd,cfd,read_num;
    char buf[BUFSIZ];
    struct sockaddr_in ssock,csock;
    socklen_t client_sock_len=sizeof(csock);
    
    char in_ip_addr[20];
    unsigned int in_ip_port;
    
    ssock.sin_family=AF_INET;
    ssock.sin_port=htons(SERVER_PORT);
    ssock.sin_addr.s_addr=INADDR_ANY;
    sfd=socket(AF_INET,SOCK_STREAM,0);
    printf("serv_fd:%d bind state:%d\n",sfd,bind(sfd,(struct sockaddr *)&ssock,sizeof(struct sockaddr_in)));
    if(listen(sfd,128)==0)
        printf("listen success\n");
    else
    {
        printf("error!\n");
    }
    
    cfd=accept(sfd,(struct sockaddr *)&csock,&client_sock_len);
    printf("client fd:%d\n",cfd);
    while(1){
        read_num=read(cfd,buf,BUFSIZ);
        inet_ntop(AF_INET,(void *)&csock.sin_addr.s_addr,in_ip_addr,INET_ADDRSTRLEN);
        in_ip_port=ntohs(csock.sin_port);
        printf("connection from %s,port %d\n",in_ip_addr,in_ip_port);
        sprintf(buf,"hello, %s:%d\n",in_ip_addr,in_ip_port);
        write(cfd,buf,strlen(buf)+1);
    }
    close(cfd);
    close(sfd);
    return 0;
}