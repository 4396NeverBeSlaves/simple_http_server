#ifndef __HTTPHANDLE_H__
#define __HTTPHANDLE_H__

#define True 1
#define False 0

#define CONNECTION_CLOSE 0
#define CONNECTION_KEEP_ALIVE 1

#define NEED_READ 0
#define NEED_WRITE 1
#define NEED_DISCONNECT 2

#define READ_BUF_SIZE 1024
#define WRITE_BUF_SIZE 4096
#define LINE_BUF_SIZE READ_BUF_SIZE
#define HOST_LENGTH 128

#define QUERY_STRING_YES 1
#define QUERY_STRING_NO 0

#define STATIC_FILE 1
#define DYNAMIC_FILE 0

 #define _DEBUG

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
    struct sockaddr_in6 sock;
} httphandle;

int accept_clients(int epfd, int lfd, httphandle *handles);
void disconnect(int epfd, httphandle *handle);
void init_httphandle(int cfd, httphandle *handle);
int do_read(int cfd, httphandle *handle);
int do_write(int cfd, httphandle *handle);
int read_line(httphandle *handle, char *line_buf);
int parse_request_line(char *line_buf, int line_size, char *method, char *file_path,char **query_string, char *protocol);
void parse_request_headers(httphandle *handle);
void send_response_headers(httphandle* handle, char* file_path);
void mount_static_doc(httphandle *handle,char *file_path);
void get_content_type(char *file_path,char *content_type);
#endif
