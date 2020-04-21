#include "httphandle.h"
#include "epoll_operation.h"
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

extern char* default_index_file;

int accept_clients(int epfd, int lfd, httphandle* handles)
{
    int cfd, cli_count = 0;
    struct sockaddr_in6 cli_sock;
    socklen_t socklen = sizeof(cli_sock);
    bzero((void*)&cli_sock, socklen);

    while (1) {
        if ((cfd = Accept(lfd, (struct sockaddr*)&cli_sock, &socklen)) < 0) {
            break;
        }
        cli_count++;
#ifdef _DEBUG
        char client_ip[INET6_ADDRSTRLEN];
        printf("Connection from %s:%d cfd:%d count:%d\n", inet_ntop(AF_INET6, &cli_sock.sin6_addr, client_ip, INET6_ADDRSTRLEN), ntohs(cli_sock.sin6_port),cfd,cli_count);
#endif
        addfd(epfd, cfd);
        handles[cfd].sock = cli_sock;
        init_httphandle(cfd, &handles[cfd]);
    }

    return cli_count;
}

void init_httphandle(int cfd, httphandle* handle)
{
    handle->fd = cfd;
    handle->read_ptr = handle->read_buf;
    // handle->write_ptr = handle->write_buf;
    handle->write_ptr = NULL;
    handle->send_file_size = 0;
    handle->connection = CONNECTION_CLOSE;
    return;
}

void disconnect(int epfd, httphandle* handle)
{
#ifdef _DEBUG
    char client_ip[INET6_ADDRSTRLEN];
    printf("%s:%d is closing!\n", inet_ntop(AF_INET6, &handle->sock.sin6_addr, client_ip, INET6_ADDRSTRLEN), ntohs(handle->sock.sin6_port));
#endif

    delfd(epfd, handle->fd);
    Close(handle->fd);
    if (handle->is_static && handle->write_buf) {
        Munmap(handle->write_buf, handle->send_file_size);

        // free(handle->write_buf);


        handle->write_buf=NULL;
    }

#ifdef _DEBUG
    printf("disconnect!\n\n\n\n\n");
    fflush(stdout);
#endif
}

int do_read(int cfd, httphandle* handle)
{
    int read_count = 0, line_size, n;
    struct stat file_status;
    char line_buf[LINE_BUF_SIZE];
    char method[10], file_path[LINE_BUF_SIZE], *query_string = NULL, protocol[20];
    file_path[0] = '.';

    /*if ((read_count = Read(handle->fd, handle->read_buf, READ_BUF_SIZE)) == 0)
        return NEED_DISCONNECT;
    //else if (read_count == READ_BUF_SIZE)
        // send_error("HTTP Error 414. The request URL is too long");
        //return NEED_DISCONNECT;*/

    while (1) {
        if ((n = recv(handle->fd, handle->read_buf + read_count, READ_BUF_SIZE - read_count, 0)) <= 0) {
            if (n == 0) {
#ifdef _DEBUG
                printf("n==0 NEED_DISCONNECT!\n");
#endif
                return NEED_DISCONNECT;
            } else if (errno == EINTR)
                continue;
            else if (errno == EAGAIN) {
#ifdef _DEBUG
                // write(STDOUT_FILENO,handle->read_buf,read_count);
                printf("errno==EAGAIN break;\n");
#endif
                break;
            } else
                perror_exit("read in do_read() error!");
        }
        read_count += n;
    }

    line_size = read_line(handle, line_buf);
    if (line_size == -1) {
// send_error("400 Bad Request");
#ifdef _DEBUG
        printf("line_size == -1 NEED_DISCONNECT!\n");
#endif
        return NEED_DISCONNECT;
    }

    if (parse_request_line(line_buf, line_size, method, &file_path[1], &query_string, protocol) == -1) {
// send_error("400 Bad Request");
#ifdef _DEBUG
        printf("parse_request_line() == -1 NEED_DISCONNECT!\n");
#endif
        return NEED_DISCONNECT;
    }

#ifdef _DEBUG
    if (query_string)
        printf("method:%s file_path:%s query_string:%s protocol:%s\n", method, file_path, query_string, protocol);
    else
        printf("method:%s file_path:%s query_string:<NULL> protocol:%s\n", method, file_path, protocol);
    fflush(stdout);
#endif

    parse_request_headers(handle);

    if (strcasecmp(file_path, "./") == 0) {
        strcpy(file_path, default_index_file);
    }

    if (stat(file_path, &file_status) < 0) {
        // send_error("404 not found");

        return NEED_DISCONNECT;
    }
    handle->send_file_size = file_status.st_size;
    if (S_ISDIR(file_status.st_mode)) {
// send_error("403 forbidden");
#ifdef _DEBUG
        printf("S_ISDIR() NEED_DISCONNECT!\n");
#endif
        return NEED_DISCONNECT;
    }
#ifdef _DEBUG
    printf("file_path:%s querying file_size:%ld\n", file_path, file_status.st_size);
    fflush(stdout);
#endif
    if (strcasecmp(method, "GET") == 0) {
        //static html doc
        if (query_string == NULL) {
            mount_static_doc(handle, file_path);
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

    send_response_headers(handle, file_path);
    //处理完之前的任务后，当前发送缓冲区为空，开始向客户端回送数据。根据发送的结果来决定是否要继续发送，如当前发送缓冲区已满可稍后在发送
    return do_write(cfd, handle);
}
int do_write(int cfd, httphandle* handle)
{
    int has_written, count;

    //若文件没有发送完成
    has_written = handle->write_ptr - handle->write_buf;
    /*
    if (has_written < handle->send_file_size) {
        //send file
        while (1) {
            if ((count = write(cfd, handle->write_ptr, handle->send_file_size)) < 0) {
                if (errno == EAGAIN)
                    break;
                else if (errno == EINTR)
                    continue;
                else
                    perror_exit("write to client error");
            }
            has_written+=count;
            handle->write_ptr += count;
            if(has_written==handle->send_file_size)
                break;
        }
    }

    if (has_written == handle->send_file_size) { //当文件发送完成后 判断连接状态来决定是否关闭连接
        if (handle->connection == CONNECTION_CLOSE)
            return NEED_DISCONNECT;
        else
            return NEED_READ;
    }

    return NEED_WRITE;
    */

    while (1) {
        if ((count = send(cfd, handle->write_ptr, handle->send_file_size, 0)) < 0) {
            if (errno == EAGAIN) {
#ifdef _DEBUG
                printf("errno == EAGAIN NEED_WRITE!\n");
                fflush(stdout);
#endif
                return NEED_WRITE;
            }

            else if (errno == EINTR)
                continue;
            else
                perror_exit("write to client error");
        }

        has_written += count;
        handle->write_ptr += count;

#ifdef _DEBUG
        write(STDOUT_FILENO, handle->write_buf, has_written);
        printf("\n");
        fflush(stdout);
#endif
        if (has_written == handle->send_file_size) {
            if (handle->connection == CONNECTION_CLOSE) {
#ifdef _DEBUG
                printf("has_written == handle->send_file_size && CONNECTION_CLOSE NEED_DISCONNECT!\n");
                fflush(stdout);
#endif
                return NEED_DISCONNECT;
            }

            else
                return NEED_READ;
        }
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

    strncpy(line_buf, handle->read_ptr, line_size);
    line_buf[line_size] = '\0';

#ifdef _DEBUG
    printf("line_size:%d line:%s", line_size, line_buf);
    fflush(stdout);
#endif
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

    return ret;
}
void parse_request_headers(httphandle* handle)
{
    char line_buf[LINE_BUF_SIZE], connection_parameter[20];
    int line_size;

#ifdef _DEBUG
    printf("------------------request headers start--------------------\n");
    fflush(stdout);
#endif

    line_size = read_line(handle, line_buf);

    while (strcmp(line_buf, "\r\n") != 0) {

        // 11 length of "Connection:"
        if (strncasecmp(line_buf, "Connection:", 11) == 0) {
            sscanf(line_buf, "%*s %s", connection_parameter);
            if (strcasecmp(connection_parameter, "Keep-alive\r\n") == 0) {
                handle->connection = CONNECTION_KEEP_ALIVE;
            }
        }

        //get other parameters

        line_size = read_line(handle, line_buf);
        // line_buf[line_size]='\0';
    }
#ifdef _DEBUG
    printf("------------------request headers end.---------------------\n");
    printf("Connection: %s\n", connection_parameter);
    fflush(stdout);
#endif
}
void get_content_type(char* file_path, char* content_type)
{
    char* suffix;

    suffix = index(&file_path[1], '.');
#ifdef _DEBUG
    printf("suffix:%s\n", suffix);
    fflush(stdout);
#endif
    if (!strcasecmp(suffix, ".html") || !strcasecmp(suffix, ".htm")) {
        strcpy(content_type, "text/html; charset=utf-8");
    } else if (!strcasecmp(suffix, ".txt") || !strcasecmp(suffix, ".log")) {
        strcpy(content_type, "text/plain; charset=utf-8");
    } else if (!strcasecmp(suffix, ".xml")) {
        strcpy(content_type, "text/xml; charset=utf-8");
    } else if (!strcasecmp(suffix, ".css")) {
        strcpy(content_type, "text/css");
    } else if (!strcasecmp(suffix, ".js")) {
        strcpy(content_type, "application/javascript");
    } else if (!strcasecmp(suffix, ".gif")) {
        strcpy(content_type, "image/gif");
    } else if (!strcasecmp(suffix, ".jpg") || !strcasecmp(suffix, ".jpeg")) {
        strcpy(content_type, "image/jpeg");
    } else if (!strcasecmp(suffix, ".png")) {
        strcpy(content_type, "image/png");
    } else if (!strcasecmp(suffix, ".json")) {
        strcpy(content_type, "application/json");
    } else if (!strcasecmp(suffix, ".pdf")) {
        strcpy(content_type, "application/pdf");
    } else if (!strcasecmp(suffix, ".mp4")) {
        strcpy(content_type, "video/mp4");
    } else if (!strcasecmp(suffix, ".mp3")) {
        strcpy(content_type, "audio/mp3");
    } else if (!strcasecmp(suffix, ".wav")) {
        strcpy(content_type, "audio/wav");
    } else if (!strcasecmp(suffix, ".ogg")) {
        strcpy(content_type, "audio/ogg");
    } else {
        strcpy(content_type, "application/octet-stream");
    }

#ifdef _DEBUG
    printf("content type:%s\n", content_type);
    fflush(stdout);
#endif
}

void send_response_headers(httphandle* handle, char* file_path)
{
    char response_headers[1024], time_string[40], content_type[30], connetcion[20] = "Keep-alive";
    int count = 0;
    time_t t1;
    struct tm* t2;

    get_content_type(file_path, content_type);
    if (handle->connection == CONNECTION_CLOSE)
        strcpy(connetcion, "Close");

    t1 = time(NULL);
    t2 = gmtime(&t1);
    strftime(time_string, 40, "%a, %d %b %Y %X GMT", t2);

    count += sprintf(response_headers, "HTTP/1.1 200 OK\r\n");
    count += sprintf(response_headers + count, "Server: X-server\r\n");
    count += sprintf(response_headers + count, "Date: %s\r\n", time_string);
    count += sprintf(response_headers + count, "Content-Length: %ld\r\n", handle->send_file_size);
    count += sprintf(response_headers + count, "Content-Type: %s\r\n", content_type);
    count += sprintf(response_headers + count, "Connection: %s\r\n", connetcion);
    if (handle->connection == CONNECTION_KEEP_ALIVE)
        count += sprintf(response_headers + count, "Keep-Alive: timeout=30, max=1000\r\n");
    count += sprintf(response_headers + count, "\r\n");

#ifdef _DEBUG
    printf("send_response_headers:\n%s", response_headers);
    fflush(stdout);
#endif
    // Write(handle->fd, response_headers, count);
    send(handle->fd, response_headers, count, 0);
}

void mount_static_doc(httphandle* handle, char* file_path)
{
    int fd;

    handle->is_static = STATIC_FILE;
    fd = open(file_path, O_RDONLY, 0);



    handle->write_buf = Mmap(0, handle->send_file_size, PROT_READ, MAP_PRIVATE, fd, 0);

    // handle->write_buf=(char*)malloc(handle->send_file_size);
    // Read(fd,handle->write_buf,handle->send_file_size);




    handle->write_ptr = handle->write_buf;
    Close(fd);
}
