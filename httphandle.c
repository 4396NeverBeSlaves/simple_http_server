#include "httphandle.h"

extern char* default_index_file;
extern vhost_list v_list;

int accept_clients(int epfd, int lfd, httphandle* handles)
{
    int cfd, cli_count = 0;
    struct sockaddr_in6 cli_sock;
    socklen_t socklen = sizeof(cli_sock);
    bzero((void*)&cli_sock, socklen);

    //当有客户端建立连接时，可能不止有一个，需要一直accept到没有新客户端连入。如果不这样监听的话，每次只接受1个，会丢失连接事件。
    while (1) {
        if ((cfd = Accept(lfd, (struct sockaddr*)&cli_sock, &socklen)) < 0) {
            break;
        }
        cli_count++;
#ifdef _DEBUG
        char client_ip[INET6_ADDRSTRLEN];
        printf("Connection from %s:%d cfd:%d count:%d\n", inet_ntop(AF_INET6, &cli_sock.sin6_addr, client_ip, INET6_ADDRSTRLEN), ntohs(cli_sock.sin6_port), cfd, cli_count);
        fflush(stdout);
        handles[cfd].sock = cli_sock;
#endif
        addfd(epfd, cfd);
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
    handle->connection = CONNECTION_KEEP_ALIVE;
    handle->host_id = -1;
    return;
}

void disconnect(int epfd, httphandle* handle)
{
#ifdef _DEBUG
    char client_ip[INET6_ADDRSTRLEN];
    printf("%s:%d is closing!\n", inet_ntop(AF_INET6, &handle->sock.sin6_addr, client_ip, INET6_ADDRSTRLEN), ntohs(handle->sock.sin6_port));
    fflush(stdout);
#endif

    delfd(epfd, handle->fd);
    Close(handle->fd);
    if (handle->is_static && handle->write_buf) {
        Munmap(handle->write_buf, handle->send_file_size);

        // free(handle->write_buf);

        handle->write_buf = NULL;
    }

#ifdef _DEBUG
    printf("disconnect!\n\n\n\n\n");
    fflush(stdout);
#endif
}

int do_read(int cfd, httphandle* handle)
{
    int read_count = 0, line_size, n;
    struct stat file_status; //请求文件的状态
    char line_buf[LINE_BUF_SIZE];
    char method[10], request_path[LINE_BUF_SIZE], file_path[LINE_BUF_SIZE], query_string[LINE_BUF_SIZE], protocol[20]; //将request_path分成文件路径与请求串
    query_string[0] = '\0'; //可以以此来判断是否有查询串

    //每次读取请求时先把指针复原
    handle->read_ptr = handle->read_buf;
    handle->write_ptr = NULL;

    /*if ((read_count = Read(handle->fd, handle->read_buf, READ_BUF_SIZE)) == 0)
        return NEED_DISCONNECT;
    //else if (read_count == READ_BUF_SIZE)
        // send_error("HTTP Error 414. The request URL is too long");
        //return NEED_DISCONNECT;*/
#ifdef _DEBUG
    char client_ip[INET6_ADDRSTRLEN];
    printf("This is %s:%d cfd:%d \n", inet_ntop(AF_INET6, &handle->sock.sin6_addr, client_ip, INET6_ADDRSTRLEN), ntohs(handle->sock.sin6_port), cfd);
    fflush(stdout);
#endif

    while (1) {
        if ((n = recv(handle->fd, handle->read_buf + read_count, READ_BUF_SIZE - read_count, 0)) <= 0) {
            if (n == 0) {

#ifdef _DEBUG
                printf("n==0(recv zero byte), NEED_DISCONNECT!\n");
                fflush(stdout);
#endif
                return NEED_DISCONNECT;

            } else if (errno == EINTR)
                continue;
            else if (errno == EAGAIN) {
#ifdef _DEBUG
                // write(STDOUT_FILENO,handle->read_buf,read_count);
                printf("errno==EAGAIN recv end! break; \n");
                fflush(stdout);
#endif
                break;
            } else {
                perror("recv in do_read() error!");
                printf(" close this connection!\n");
                return NEED_DISCONNECT;
            }
        }
        read_count += n;
#ifdef _DEBUG
        handle->read_buf[read_count] = '\0';
        printf("read_count:%d content:\n%s", read_count, handle->read_buf);
        // write(STDOUT_FILENO,handle->read_buf,read_count);

        fflush(stdout);
#endif
    }

    line_size = read_line(handle, line_buf);
    if (line_size == -1) {
#ifdef _DEBUG
        printf("line_size == -1 NEED_DISCONNECT!\n");
        fflush(stdout);
#endif
        return send_error_page(handle, 400, RESPONSE_STATUS_400_BAD_REQUEST);
    }

    if (parse_request_line(handle, line_buf, method, request_path, query_string, protocol) == -1) {
#ifdef _DEBUG
        printf("parse_request_line() == -1 NEED_DISCONNECT!\n");
        fflush(stdout);
#endif
        return send_error_page(handle, 400, RESPONSE_STATUS_400_BAD_REQUEST);
    }

#ifdef _DEBUG
    if (query_string[0])
        printf("method:%s request_path:%s query_string:%s protocol:%s\n", method, request_path, query_string, protocol);
    else
        printf("method:%s request_path:%s query_string:<NULL> protocol:%s\n", method, request_path, protocol);
    fflush(stdout);
#endif

    parse_request_headers(handle);

    //处理请求虚拟主机在本地中文件的路径
    if (handle->host_id == -1) {
#ifdef _DEBUG
        printf("host not found! NEED_DISCONNECT!\n");
        fflush(stdout);
#endif
        return send_error_page(handle, 400, RESPONSE_STATUS_400_BAD_REQUEST);
    }

    strcpy(file_path, v_list.vhosts[handle->host_id].www_root);
    if(strcmp(request_path,"/")==0)
        strcat(file_path,default_index_file);
    else
        strcat(file_path, request_path);
    
    if (stat(file_path, &file_status) < 0) {
#ifdef _DEBUG
        printf("file not found:%s NEED_DISCONNECT!\n", file_path);
        fflush(stdout);
#endif
        return send_error_page(handle, 404, RESPONSE_STATUS_404_NOT_FOUND);
    }
    handle->send_file_size = file_status.st_size;
#ifdef _DEBUG
    printf("file_path:%s, request file_size:%ld.\n", file_path, file_status.st_size);
    fflush(stdout);
#endif
    if (S_ISDIR(file_status.st_mode)) {
#ifdef _DEBUG
        printf("S_ISDIR() 403 forbidden NEED_DISCONNECT!\n");
        fflush(stdout);
#endif
        return send_error_page(handle, 403, RESPONSE_STATUS_403_Forbidden);
    }

    //判断请求方法
    if (strcasecmp(method, "GET") == 0) {
        //若没有查询字符串，即是静态文档
        if (query_string[0] == '\0') {
            mount_static_doc(handle, file_path);
        } else { //dynamic doc
            ;
        }
    } else if (strcasecmp(method, "POST") == 0) {
        //
        ;
    } else {
        return send_error_page(handle, 501, RESPONSE_STATUS_501_NOT_IMPLEMENTED);
    }

    send_response_headers(handle, file_path, 200, RESPONSE_STATUS_200_OK);
    //处理完之前的任务后，当前socket发送缓冲区为空，开始向客户端回送数据。根据发送的结果来决定是否要继续发送，如当前发送缓冲区已满可稍后在发
    return do_write(cfd, handle);
}
int do_write(int cfd, httphandle* handle)
{
    int has_written, count;

    //若文件没有发送完成
    has_written = handle->write_ptr - handle->write_buf;

    while (1) {
        if ((count = send(cfd, handle->write_ptr, handle->send_file_size - has_written, MSG_NOSIGNAL)) < 0) {
            if (errno == EAGAIN) {
#ifdef _DEBUG
                printf("errno == EAGAIN NEED_WRITE!\n");
                fflush(stdout);
#endif
                return NEED_WRITE;
            }

            else if (errno == EINTR)
                continue;
            else {
                perror("send to client error");
                printf(" close this connection!\n");
                return NEED_DISCONNECT;
            }
        }

        has_written += count;
        handle->write_ptr += count;

#ifdef _DEBUG
        printf("has_written:%d\n", has_written);
        printf("----------------------current written content head:%.*s\n----------------------current written content end:%.*s\n", 100, handle->write_ptr - count, 100, handle->write_ptr - 100);
        // write(STDOUT_FILENO, handle->write_ptr-count, 200);

        printf("\n");
        fflush(stdout);
#endif
        if (has_written == handle->send_file_size) {
            // 如果客户端是短连接就直接关闭,若是长连接的话返回NEED_READ继续监听该客户端的请求
            if (handle->connection == CONNECTION_CLOSE) {
#ifdef _DEBUG
                printf("has_written == handle->send_file_size && CONNECTION_CLOSE NEED_DISCONNECT!\n");
                fflush(stdout);
#endif
                return NEED_DISCONNECT;
            } else {
#ifdef _DEBUG
                printf("has_written == handle->send_file_size && CONNECTION_KEEP_ALIVE NEED_READ!\n");
                fflush(stdout);
#endif

                return NEED_READ;
            }
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
    line_size = index_ptr  - handle->read_ptr + 1;

    // strncpy(line_buf, handle->read_ptr, line_size);
    memcpy(line_buf, handle->read_ptr, line_size);
    if(strncmp(line_buf,"\r\n",2)==0)//!!!!!
        line_buf[line_size] = '\0'; 
    else
        line_buf[line_size-2] = '\0';   //将字符串中\r\n置为\0
    
#ifdef _DEBUG
    printf("line_size:%d line:%s\n", line_size, line_buf);
    fflush(stdout);
#endif
    handle->read_ptr = index_ptr + 1;
    return line_size;
}

//-1 wrong;
int parse_request_line(httphandle* handle, char* line_buf, char* method, char* request_path, char* query_string, char* protocol)
{
    int ret;
    char* query_string_index = NULL;

    ret = sscanf(line_buf, "%s %s %s", method, request_path, protocol);
    //若读不到三个参数、请求方法不是GET或POST、http小于0.9或协议大于1.1，那么就返回错误
    if (ret < 3 || (strcasecmp(method, "GET") && strcasecmp(method, "POST")) || strcmp(&protocol[5], "0.9") < 0 || strcmp(&protocol[5], "1.1") > 0)
        return -1;
    if (strcmp(&protocol[5], "1.1") < 0) {
        handle->connection = CONNECTION_CLOSE;
#ifdef _DEBUG
        printf("client http protocol <1.1. connection=CONNECTION_CLOSE\n");
        fflush(stdout);
#endif
    }

    //将request_path分成两段，把原来路径与串中间的?换成'\0'隔开，'\0'后边为查询串,再将其复制到query_string中
    query_string_index = index(request_path, '?');
    if (query_string_index) {
        query_string_index[0] = '\0';
        strcpy(query_string, query_string_index + 1);
        // #ifdef _DEBUG
        //         printf("query_string:%s\n", query_string);
        //         fflush(stdout);
        // #endif
    }

    return ret;
}
void parse_request_headers(httphandle* handle)
{
    char line_buf[LINE_BUF_SIZE], connection_parameter[20],*port_index;

#ifdef _DEBUG
    printf("------------------request headers start--------------------\n");
    fflush(stdout);
#endif

    read_line(handle, line_buf);

    while (strcmp(line_buf, "\r\n") != 0) {

        //客户端有无请求长连接。11 length of "Connection:"
        if (strncasecmp(line_buf, "Connection:", 11) == 0) {
            sscanf(line_buf, "%*s %s", connection_parameter);
            if (strncasecmp(connection_parameter, "Keep-alive", 11) == 0) {
                handle->connection = CONNECTION_KEEP_ALIVE;
#ifdef _DEBUG
                printf("set handle->connection -> CONNECTION_KEEP_ALIVE\n");
                fflush(stdout);
#endif
            }
        }

        if (strncasecmp(line_buf, "Host:", 5) == 0) {
            if((port_index=rindex(line_buf,':')))//若请求主机后面带有端口号，那么将 ':' -> '\0'，得出主机字符串，以免端口号影响之后查询www-root目录。
                *(port_index)='\0';   
            handle->host_id = get_vhost_id(&line_buf[6]);   //查询请求的host的id，若未查到则为-1
#ifdef _DEBUG
            printf("host_id:%d Host: %s\n", handle->host_id, &line_buf[6]);
#endif
        }

        //get other parameters

        read_line(handle, line_buf);
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

    suffix = rindex(file_path, '.');
#ifdef _DEBUG
    printf("suffix:%s\n", suffix);
    fflush(stdout);
#endif
    if (!suffix) {
        strcpy(content_type, "application/octet-stream");
        return;
    }
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
    } else if (!strcasecmp(suffix, ".ico")) {
        strcpy(content_type, "image/x-icon");
    } else if (!strcasecmp(suffix, ".json")) {
        strcpy(content_type, "application/json");
    } else if (!strcasecmp(suffix, ".pdf")) {
        strcpy(content_type, "application/pdf");
    } else if (!strcasecmp(suffix, ".mp4")) {
        strcpy(content_type, "video/mp4");
    } else if (!strcasecmp(suffix, ".webm")) {
        strcpy(content_type, "video/webm");
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

void send_response_headers(httphandle* handle, char* file_path, int response_status_code, char* response_status_string)
{
    char response_headers[1024], time_string[40], content_type[30], connetcion[20] = "Keep-alive";
    int count = 0, sent_count = 0, n;
    time_t t1;
    struct tm* t2;

    get_content_type(file_path, content_type);
    if (handle->connection == CONNECTION_CLOSE || response_status_code != 200)
        strcpy(connetcion, "Close");

    t1 = time(NULL);
    t2 = gmtime(&t1);
    strftime(time_string, 40, "%a, %d %b %Y %X GMT", t2);

    count += sprintf(response_headers, "HTTP/1.1 %d %s\r\n", response_status_code, response_status_string);
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

    while (1) {
        //设置标志位为MSG_NOSIGNAL|MSG_MORE，1、可以防止收到SIGPIPE信号进程退出，2、等待后方要发送的数据，将响应首部与文档一起发送
        if ((n = send(handle->fd, response_headers, count, (MSG_NOSIGNAL | MSG_MORE))) < 0) {
#ifdef _DEBUG
            printf("sent_count<0!\n");
            fflush(stdout);
#endif
            if (errno == EINTR || errno == EAGAIN) {
#ifdef _DEBUG
                printf("forcedly send! errno == EINTR || errno == EAGAIN in send_response_header()\n");
                fflush(stdout);
#endif
                continue;
            }
        }
        sent_count += n;
        if (sent_count == count) {
#ifdef _DEBUG
            printf("All headers have sent!\n");
            fflush(stdout);
#endif
            break;
        }
    }
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

int send_error_page(httphandle* handle, int response_status_code, char* response_status_string)
{
    char error_page_file_name[15];
    struct stat file_stat;

    sprintf(error_page_file_name, "./%d.html", response_status_code);
    if (stat(error_page_file_name, &file_stat) < 0) {
        char error_page[200];
        int sent_count = 0, n;

        // generate_error_page
        sprintf(error_page, "<html><head><title>%d %s</title></head><body><h1>%d %s</h1><hr><em>Powered by X-server.</em></body></html>", response_status_code, response_status_string, response_status_code, response_status_string);
        handle->send_file_size = strlen(error_page);
        send_response_headers(handle, error_page_file_name, response_status_code, response_status_string);
        while (sent_count < handle->send_file_size) {
            if ((n = send(handle->fd, error_page + sent_count, handle->send_file_size - sent_count, MSG_NOSIGNAL)) < 0) {
                if (errno == EINTR || errno == EAGAIN) {
#ifdef _DEBUG
                    printf("forcedly send! errno == EINTR || errno == EAGAIN in send_error_page()\n");
                    fflush(stdout);
#endif
                    continue;
                }
            }
            sent_count += n;
        }
        return NEED_DISCONNECT;
    } else {
        handle->send_file_size = file_stat.st_size;
        mount_static_doc(handle, error_page_file_name);
        send_response_headers(handle, error_page_file_name, response_status_code, response_status_string);
        return do_write(handle->fd, handle);
    }
}
