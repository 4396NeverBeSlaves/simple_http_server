#include "httphandle.h"
#include "epoll_operation.h"
#include "wrap.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <time.h>

extern char* default_index_file;

void accept_client(int epfd, int lfd, httphandle* handles)
{
    int cfd;
    struct sockaddr_in6 cli_sock;
    socklen_t socklen = sizeof(cli_sock);

    bzero((void*)&cli_sock, socklen);
    cfd = Accept(lfd, (struct sockaddr*)&cli_sock, &socklen);
    addfd(epfd, cfd);

    init_httphandle(cfd, &handles[cfd]);
}

void init_httphandle(int cfd, httphandle* handle)
{
    handle->fd = cfd;
    handle->read_ptr = handle->read_buf;
    // handle->write_ptr = handle->write_buf;
    handle->send_file_size=0;
    handle->connection = CONNECTION_KEEP_ALIVE;
    return;
}

void disconnect(int epfd, int cfd)
{
    delfd(epfd, cfd);
    Close(cfd);
}

int do_read(int cfd, httphandle* handle)
{
    int read_count, line_size;
    struct stat file_status;
    char line_buf[LINE_BUF_SIZE];
    char method[10], file_path[LINE_BUF_SIZE], *query_string = NULL, protocol[20];
    file_path[0] = '.';

    if ((read_count = Read(handle->fd, handle->read_buf, READ_BUF_SIZE)) == 0)
        return NEED_DISCONNECT;
    else if (read_count == READ_BUF_SIZE)
        // send_error("HTTP Error 414. The request URL is too long");
        return NEED_DISCONNECT;

    line_size = read_line(handle, line_buf);
    if (line_size == -1)
        // send_error("400 Bad Request");
        return NEED_DISCONNECT;

    if (parse_request_line(line_buf, line_size, method, &file_path[1], &query_string, protocol) == -1) {
        // send_error("400 Bad Request");
        return NEED_DISCONNECT;
    }

    parse_request_header(handle);

    if (strcasecmp(file_path, "./") == 0) {
        strcpy(file_path, default_index_file);
    }

    // if(query_string)
    //     printf("method:%s file_path:%s query_string:%s protocol:%s\n",method,file_path,query_string,protocol);
    // else
    //     printf("method:%s file_path:%s query_string:<NULL> protocol:%s\n",method,file_path,protocol);
    if (stat(file_path, &file_status) < 0) {
        // send_error("404 not found");
        return NEED_DISCONNECT;
    }
    if (S_ISDIR(file_status.st_mode)) {
        // send_error("403 forbidden");
        return NEED_DISCONNECT;
    }

    printf("file_path:%s querying file_size:%ld\n",file_path, file_status.st_size);
    if (strcasecmp(method, "GET") == 0) {
        //static html doc
        if (query_string == NULL) {
            send_static_doc(handle, file_path, file_status.st_size);
        } else { //dynamic doc
            ;
        }
    } else if (strcasecmp(method, "POST") == 0) {
        //
        ;
    } else {
        // send_error(" 501 Not Implemented");
        return NEED_DISCONNECT;
    }

    //处理完之前的任务后，当前发送缓冲区为空，开始向客户端回送数据。根据发送的结果来决定是否要继续发送，如当前发送缓冲区已满可稍后在发送
    return do_write(cfd,handle);
}
int do_write(int cfd, httphandle* handle)
{
    int has_written;

    //send file
    while (1)
    {
        if((has_written=write(cfd,handle->write_ptr,handle->send_file_size))<0){
            if(errno==EAGAIN)
                break;
            else if(errno==EINTR)
                continue;
            else
                perror_exit("write to client error");
        }
        handle->write_ptr+=has_written;
    }
    
}

int read_line(httphandle* handle, char* line_buf)
{
    char* index_ptr;
    int line_size;

    index_ptr = index(handle->read_ptr, '\n');
    if (index_ptr == NULL)
        return -1;
    line_size = index_ptr + 1 - handle->read_ptr;

    // write(STDOUT_FILENO,handle->read_ptr,line_size);

    strncpy(line_buf, handle->read_ptr, line_size);
    line_buf[line_size]='\0';

    // printf("line_size:%d line:%s",line_size,line_buf);

    handle->read_ptr = index_ptr + 1;
    return line_size;
}

//-1 wrong;
int parse_request_line(char* line_buf, int line_size, char* method, char* file_path, char** query_string, char* protocol)
{
    int ret;

    ret = sscanf(line_buf, "%s %s %s", method, file_path, protocol);
    if (ret < 3)
        return -1;
    *query_string = index(file_path, '?');
    if (*query_string) {
        (*query_string)[0] = '\0'; //set ? to \0
        *query_string += 1;
    }
    // printf("ret:%d %s %s\n",ret,file_path,*query_string);

    return ret;
}
void parse_request_header(httphandle* handle)
{
    char line_buf[LINE_BUF_SIZE], connection_parameter[20];
    int line_size;

    line_size = read_line(handle, line_buf);

    // printf("------------------request header start--------------------\n");

    while (strcmp(line_buf, "\r\n") != 0){
        // printf("%s",line_buf);

        // 11 length of "Connection:"
        if (strncasecmp(line_buf, "Connection:", 11) == 0) {
            sscanf(line_buf, "%*s %s", connection_parameter);
            if (strcasecmp(connection_parameter, "Close\r\n") == 0) {
                handle->connection = CONNECTION_CLOSE;
            }
        }

        //get other parameters
    
        line_size = read_line(handle, line_buf);
        // line_buf[line_size]='\0';
    }
    // printf("------------------request header end.---------------------\n");
    // printf("Connection: %s\n", connection_parameter);
}
void get_content_type(char *file_path,char *content_type){
	;
}
void send_static_doc(httphandle* handle, char* file_path, int file_size)
{
    int fd;
    char response_header[1024],time_string[40],content_type[50],connetcion[20]="Keep-alive";
    time_t t1;
    struct tm *t2;

    fd=open(file_path,O_RDONLY);
    handle->write_buf= Mmap(0,file_size,PROT_READ,MAP_PRIVATE,fd,0);
    handle->write_ptr=handle->write_buf;
    handle->send_file_size=file_size;
    Close(fd);
    // printf("%s\n",handle->write_buf);

    get_content_type(file_path,content_type);
    if(handle->connection==CONNECTION_CLOSE)
        strcpy(connetcion,"Close");
    
    t1=time(NULL);
    t2=gmtime(&t1);
    strftime(time_string,40,"%a, %d %b %Y %X GMT",t2);

    sprintf(response_header,"HTTP/1.1 200 OK\r\n");
    sprintf(response_header+strlen(response_header),"Server: X-server\r\n");
    sprintf(response_header+strlen(response_header),"Date: %s\r\n",time_string);
    sprintf(response_header+strlen(response_header),"Content-Length: %ld\r\n",handle->send_file_size);
    sprintf(response_header+strlen(response_header),"Content-Type: %s\r\n",content_type);
    sprintf(response_header+strlen(response_header),"Connection: %s\r\n",connetcion);

}

