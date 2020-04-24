#ifndef __HTTPHANDLE_H__
#define __HTTPHANDLE_H__

//http/1.1默认开启长连接，若在请求行检测到客户端http协议版本小于1.1，则先设置为短连接。如果在请求头部检测到Connection:Keep-alive,则设置为长连接
#define CONNECTION_CLOSE 0
#define CONNECTION_KEEP_ALIVE 1

#define NEED_READ 0
#define NEED_WRITE 1
#define NEED_DISCONNECT 2

#define READ_BUF_SIZE 1024
#define WRITE_BUF_SIZE 4096
#define LINE_BUF_SIZE 1024
#define HOST_LENGTH 128

#define QUERY_STRING_YES 1
#define QUERY_STRING_NO 0

#define STATIC_FILE 1
#define DYNAMIC_FILE 0

#define RESPONSE_STATUS_200_OK "OK"
#define RESPONSE_STATUS_404_NOT_FOUND "Not Found"
#define RESPONSE_STATUS_400_BAD_REQUEST "Bad Request"
#define RESPONSE_STATUS_403_Forbidden "Forbidden"
#define RESPONSE_STATUS_414_REQUEST_URI_TOO_LARGE "Request-URI Too Large"
#define RESPONSE_STATUS_501_NOT_IMPLEMENTED "Not Implemented"

// #define _DEBUG

#include<arpa/inet.h>
typedef struct httphandle
{
    int fd;
    char read_buf[READ_BUF_SIZE];
    char *read_ptr;
    char *write_buf;
    char *write_ptr;
    long send_file_size;
    char is_static;
    char connection;
    char host[HOST_LENGTH];
    char protocol_ver[4];
    struct sockaddr_in6 sock;
} httphandle;

int accept_clients(int epfd, int lfd, httphandle *handles);
void disconnect(int epfd, httphandle *handle);
void init_httphandle(int cfd, httphandle *handle);
int do_read(int cfd, httphandle *handle);
int do_write(int cfd, httphandle *handle);
int read_line(httphandle *handle, char *line_buf);
int parse_request_line(httphandle *handle,char* line_buf, char *method, char *file_path,char **query_string, char *protocol);
void parse_request_headers(httphandle *handle);
void send_response_headers(httphandle* handle, char* file_path,int response_status_code,char *response_status_string);
void mount_static_doc(httphandle *handle,char *file_path);
void get_content_type(char *file_path,char *content_type);
int send_error_page(httphandle *handle,int response_status_code,char *response_status_string);
#endif
