#ifndef __HTTPHANDLE_H__
#define __HTTPHANDLE_H__

#include "epoll_operation.h"
#include "vhost_handle.h"
#include "wrap.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

//http/1.1默认开启长连接，若在请求行检测到客户端http协议版本小于1.1，则先设置为短连接。如果在请求头部检测到Connection:Keep-alive,则设置为长连接
#define CONNECTION_CLOSE 0
#define CONNECTION_KEEP_ALIVE 1

#define NEED_READ 0
#define NEED_WRITE 1
#define NEED_DISCONNECT 2

#define POST_DATA_BUF_SIZE 1048576 //=1024*1024
#define READ_BUF_SIZE 262144 //256*1024
#define WRITE_BUF_SIZE 1048576
#define LINE_BUF_SIZE 2048

#define REQUEST_GET 0
#define REQUEST_POST 1
#define REQUEST_OTHER 2

#define QUERY_STRING_YES 1
#define QUERY_STRING_NO 0

#define STATIC_FILE 0
#define DYNAMIC_FILE 1

//post data form
#define X_WWW_FORM_URLENCODED 0
#define FORM_DATA 1 //do not support

#define RESPONSE_STATUS_200_OK "OK"
#define RESPONSE_STATUS_404_NOT_FOUND "Not Found"
#define RESPONSE_STATUS_400_BAD_REQUEST "Bad Request"
#define RESPONSE_STATUS_403_Forbidden "Forbidden"
#define RESPONSE_STATUS_414_REQUEST_URI_TOO_LARGE "Request-URI Too Large"
#define RESPONSE_STATUS_501_NOT_IMPLEMENTED "Not Implemented"
#define RESPONSE_STATUS_503_SERVICE_UNAVAILABLE "Service Unavailable"
#define RESPONSE_STATUS_505_HTTP_VERSION_NOT_SUPPORTED "HTTP Version Not Supported"

// #define _DEBUG

typedef struct httphandle {
    int fd;
    int host_id; //请求的host在v_list中的下标，当请求首部里面没有Host字段或者找不到该主机时为-1。
    char connection;    //是否为长连接
    char* read_buf;
    char* read_ptr; //始终指向未处理的第一个字节
    int read_data_length; //实际读取到的长度
    char* write_buf;
    char* write_ptr;
    int send_file_size; //要发送给客户端实际数据大小（有时会包括部分响应头部数据）
    char request_method; //0 GET; 1 POST; 2 OTHER
    char static_dynamic; //要发送给客户端:若是静态文件则为0，动态文件则==1

    int post_content_length; //客户端在请求首部content-length中post的长度

    char* post_data;    //cgi程序发送来的数据
    char post_content_type; //cgi程序发送的数据类型
    int response_headers_length; //来自cgi程序发送来的数据中部分响应首部的长度.在发送响应首部时，应减去该部分得出http文档真正长度

#ifdef _DEBUG
    struct sockaddr_in6 sock;
#endif
} httphandle;

int accept_clients(int epfd, int lfd, httphandle* handles);
void disconnect(int epfd, httphandle* handle);
void init_httphandle(int cfd, httphandle* handle);
int do_read(int cfd, httphandle* handle);
int do_write(int cfd, httphandle* handle);
int read_line(httphandle* handle, char* line_buf);
int parse_request_line(httphandle* handle, char* line_buf, char* method, char* request_path, char* query_string, char* protocol);
void parse_request_headers(httphandle* handle);
void send_response_headers(httphandle* handle, char* request_path, int response_status_code, char* response_status_string);
void check_static_dynamic(httphandle* handle, char* path);
void mount_static_doc(httphandle* handle, char* file_path);
void get_content_type(char* file_path, char* content_type);
int send_error_page(httphandle* handle, int response_status_code, char* response_status_string);

int run_cgi_get(httphandle* handle, char* file_path, char* query_string);
int run_cgi_post(httphandle* handel, char* file_path);
#endif
