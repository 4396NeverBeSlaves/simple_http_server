#include "httphandle.h"
#include <sys/wait.h>

extern char** __environ;
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
    handle->read_buf = malloc(READ_BUF_SIZE);
    handle->write_ptr = NULL;
    handle->send_file_size = 0;
    handle->connection = CONNECTION_KEEP_ALIVE;
    handle->static_dynamic = STATIC_FILE;
    handle->host_id = -1;
    handle->post_data = NULL;

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
    free(handle->read_buf);
    if (handle->write_buf) {
        if (handle->static_dynamic == STATIC_FILE)
            Munmap(handle->write_buf, handle->send_file_size);
        else
            free(handle->write_buf);
        handle->write_buf = NULL;
    }
    if (handle->post_data) {
        free(handle->post_data);
        handle->post_data = NULL;
    }
#ifdef _DEBUG
    printf("disconnect!\n\n\n\n\n");
    fflush(stdout);
#endif
}
void get_request_method(httphandle* handle)
{
    char method[5];

    recv(handle->fd, method, 5, MSG_PEEK);
    if (strncasecmp(method, "POST", 4) == 0) {
        handle->request_method = REQUEST_POST;
    } else if (strncasecmp(method, "GET", 3) == 0) {
        handle->request_method = REQUEST_GET;
    } else {
        handle->request_method = REQUEST_OTHER;
    }
}
int do_read(int cfd, httphandle* handle)
{
    int read_count = 0, line_size, n;
    struct stat file_status; //请求文件的状态
    char line_buf[LINE_BUF_SIZE];
    char method[10], request_path[LINE_BUF_SIZE], file_path[LINE_BUF_SIZE], query_string[LINE_BUF_SIZE], protocol[20]; //将request_path分成文件路径与请求串
    query_string[0] = '\0'; //方便设置环境变量

    //每次读取请求时先把指针复原
    handle->read_ptr = handle->read_buf;
    handle->write_ptr = NULL;

    get_request_method(handle);
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
                if (read_count == READ_BUF_SIZE) {
#ifdef _DEBUG
                    printf("n==0(recv zero byte) && read_count==LINE_BUF_SIZE, break!\n");
                    fflush(stdout);
#endif
                    break;
                } else {
#ifdef _DEBUG
                    printf("n==0(recv zero byte) && read_count < LINE_BUF_SIZE, NEED_DISCONNECT!\n");
                    fflush(stdout);
#endif
                    return NEED_DISCONNECT;
                }

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
    }
#ifdef _DEBUG
    handle->read_buf[read_count] = '\0';
    printf("read_count:%d content:\n%.*s", read_count, 500, handle->read_buf);
    fflush(stdout);
#endif
    handle->read_data_length = read_count;
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

#ifdef _DEBUG
    printf("actual body length:%d\n", (int)(handle->read_data_length - (handle->read_ptr - handle->read_buf)));
    fflush(stdout);
#endif
    if (handle->request_method == REQUEST_POST && (handle->read_data_length - (handle->read_ptr - handle->read_buf)) != handle->post_content_length) {
#ifdef _DEBUG
        printf("Length doesn't macth! NEED_DISCONNECT!\n");
        fflush(stdout);
#endif
        return send_error_page(handle, 400, RESPONSE_STATUS_400_BAD_REQUEST);
    }
    //处理请求虚拟主机在本地中文件的路径
    if (handle->host_id == -1) {
#ifdef _DEBUG
        printf("host not found! NEED_DISCONNECT!\n");
        fflush(stdout);
#endif
        return send_error_page(handle, 400, RESPONSE_STATUS_400_BAD_REQUEST);
    }

    strcpy(file_path, v_list.vhosts[handle->host_id].www_root);
    if (strcmp(request_path, "/") == 0)
        strcat(file_path, default_index_file);
    else
        strcat(file_path, request_path);

    if (stat(file_path, &file_status) < 0) {
#ifdef _DEBUG
        printf("file not found:%s NEED_DISCONNECT!\n", file_path);
        fflush(stdout);
#endif
        return send_error_page(handle, 404, RESPONSE_STATUS_404_NOT_FOUND);
    }
#ifdef _DEBUG
    printf("file_path:%s, request file_size:%ld\n", file_path, file_status.st_size);
    fflush(stdout);
#endif
    if (S_ISDIR(file_status.st_mode)) {
#ifdef _DEBUG
        printf("S_ISDIR() 403 forbidden NEED_DISCONNECT!\n");
        fflush(stdout);
#endif
        return send_error_page(handle, 403, RESPONSE_STATUS_403_Forbidden);
    }
    //请求文件存在且不是文件夹的话,依据请求路径来检查是否为动态文档,是否以.cgi后缀
    check_static_dynamic(handle, request_path);

    if (handle->static_dynamic == STATIC_FILE) { //若是静态文档就直接读取文件再发送
        handle->send_file_size = file_status.st_size;
        mount_static_doc(handle, file_path);
    } else {
        //判断请求方法
        if (strcasecmp(method, "GET") == 0) {
            if (run_cgi_get(handle, file_path, query_string) == NEED_DISCONNECT) {
                send_error_page(handle, 503, RESPONSE_STATUS_503_SERVICE_UNAVAILABLE);
                return NEED_DISCONNECT;
            }
        } else if (strcasecmp(method, "POST") == 0) {
            //handle->read_ptr 此时指向还未读取的请求体第一个字节，读取剩下的数据
            if (run_cgi_post(handle, file_path) == NEED_DISCONNECT) {
                send_error_page(handle, 503, RESPONSE_STATUS_503_SERVICE_UNAVAILABLE);
                return NEED_DISCONNECT;
            }
        } else {
            return send_error_page(handle, 501, RESPONSE_STATUS_501_NOT_IMPLEMENTED);
        }
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
        int dis_length = 100;
        printf("has_written:%d\n", has_written);
        if (has_written < dis_length)
            dis_length = has_written;
        printf("----------------------current written content head:%.*s\n----------------------current written content end:%.*s\n", dis_length, handle->write_ptr - count, dis_length, handle->write_ptr - dis_length);
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
    line_size = index_ptr - handle->read_ptr + 1;

    // strncpy(line_buf, handle->read_ptr, line_size);
    memcpy(line_buf, handle->read_ptr, line_size);
    if (strncmp(line_buf, "\r\n", 2) == 0) //!!!!!若是请求首部最后一行
        line_buf[line_size] = '\0';
    else
        line_buf[line_size - 2] = '\0'; //将字符串中\r\n置为\0

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
    char line_buf[LINE_BUF_SIZE], connection_parameter[20] = { 0 }, *port_index = NULL;

#ifdef _DEBUG
    printf("------------------request headers start--------------------\n");
    fflush(stdout);
#endif

    read_line(handle, line_buf);

    while (strcmp(line_buf, "\r\n") != 0) {

        //客户端有无请求长连接。11 length of "Connection:"
        if (!connection_parameter[0] && strncasecmp(line_buf, "Connection:", 11) == 0) {
            sscanf(line_buf, "%*s %s", connection_parameter);
            if (strncasecmp(connection_parameter, "Keep-alive", 11) == 0) {
                handle->connection = CONNECTION_KEEP_ALIVE;
#ifdef _DEBUG
                printf("set handle->connection -> CONNECTION_KEEP_ALIVE\n");
                fflush(stdout);
#endif
            }
        }

        if (handle->host_id == -1 && strncasecmp(line_buf, "Host:", 5) == 0) {
            if ((port_index = rindex(line_buf, ':'))) //若请求主机后面带有端口号，那么将 ':' -> '\0'，得出主机字符串，以免端口号影响之后查询www-root目录。
                *(port_index) = '\0';
            handle->host_id = get_vhost_id(&line_buf[6]); //查询请求的host的id，若未查到则为-1
#ifdef _DEBUG
            printf("host_id:%d Host: %s\n", handle->host_id, &line_buf[6]);
#endif
        }

        if (handle->request_method == REQUEST_POST) {
            if (strncasecmp(line_buf, "Content-Length:", 15) == 0) {
                handle->post_content_length = atoi(index(line_buf, ':') + 2);
                // while (handle->post_data == NULL) { //若之前post过就不用再次分配post data存储空间
                //     handle->post_data = malloc(POST_DATA_BUF_SIZE);
                // }
            }
            if (strncasecmp(line_buf, "Content-Type: application/x-www-form-urlencoded", 48) == 0) {
                handle->post_content_type = X_WWW_FORM_URLENCODED;
            } else {
                handle->post_content_type = FORM_DATA;
            }
        }
        //get other parameters

        read_line(handle, line_buf);
    }
#ifdef _DEBUG
    if (handle->request_method == REQUEST_POST)
        printf("post_content_length:%d content type code:%d\n", handle->post_content_length, handle->post_content_type);

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
    printf("file content type:%s\n", content_type);
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
    //正常返回200若是静态文件 handle->static_dynamic==0，若是动态文件 handle->static_dynamic>0，将减去动态生成的请求请求首部长度。若返回状态码非200，则直接返回异常页面长度
    if (response_status_code == 200)
        count += sprintf(response_headers + count, "Content-Length: %d\r\n", handle->send_file_size - handle->response_headers_length);
    else
        count += sprintf(response_headers + count, "Content-Length: %d\r\n", handle->send_file_size);
    count += sprintf(response_headers + count, "Connection: %s\r\n", connetcion);
    if (handle->connection == CONNECTION_KEEP_ALIVE && response_status_code == 200)
        count += sprintf(response_headers + count, "Keep-Alive: timeout=30, max=1000\r\n");
    //1、静态文件返回 2、动态文件异常返回
    if (handle->static_dynamic == STATIC_FILE || (handle->static_dynamic == DYNAMIC_FILE && response_status_code != 200)) {
        count += sprintf(response_headers + count, "Content-Type: %s\r\n", content_type);
        count += sprintf(response_headers + count, "\r\n");
    }
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

    handle->static_dynamic = STATIC_FILE;
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

void check_static_dynamic(httphandle* handle, char* request_path)
{
    char* suffix;

    suffix = rindex(request_path, '.');
    if (suffix && strcmp(suffix, ".cgi") == 0) {
        handle->static_dynamic = DYNAMIC_FILE;
    } else
        handle->static_dynamic = STATIC_FILE;
#ifdef _DEBUG
    if (handle->static_dynamic == DYNAMIC_FILE) {
        printf("DYNAMIC_FILE\n");
    } else {
        printf("STATIC_FILE\n");
    }
    fflush(stdout);
#endif
}

int run_cgi_get(httphandle* handle, char* file_path, char* query_string)
{
    pid_t pid;
    char *dynamic_doc_index, *dynamic_file_buf;
    int status, pipefd[2], read_count;
    if (pipe(pipefd) < 0) {
        perror("pipe() error");
    }
    dynamic_file_buf = malloc(WRITE_BUF_SIZE);
    if (dynamic_file_buf == NULL) {
        perror("allocate dynamic_file_buf failed! NEED_DISCONNECT");
        return NEED_DISCONNECT;
    }
    if ((pid = fork()) < 0) {
        perror("fork() error! NEED_DISCONNECT");
        return NEED_DISCONNECT;
    } else if (pid == 0) {
        Close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        setenv("QUERY_STRING", query_string, 1);
        setenv("REQUEST_METHOD", "GET", 1);
        execle(file_path, file_path, NULL, __environ);
    } else {
        Close(pipefd[1]);
        read_count = Read(pipefd[0], dynamic_file_buf, WRITE_BUF_SIZE);
        wait(&status);

        dynamic_file_buf[read_count] = '\0';
        Close(pipefd[0]);
#ifdef _DEBUG
        printf("%d bytes read from cgi:%.*s\n", read_count, 200, dynamic_file_buf);
        if (WIFEXITED(status)) {
            printf("cgi exited with %d\n", WEXITSTATUS(status));
        }
#endif
        if (WIFSIGNALED(status)) {
            printf("cgi exited with error: %d\n", WTERMSIG(status));
            fflush(stdout);
            return NEED_DISCONNECT;
        }

        dynamic_doc_index = strstr(dynamic_file_buf, "\r\n\r\n");
        if (!dynamic_doc_index) {
            perror("GET. Bad dynamic_doc! NEED_DISCONNECT");
            return NEED_DISCONNECT;
        }
        handle->response_headers_length = dynamic_doc_index + 4 - dynamic_file_buf; //cgi程序发送请求首部的大小 要算上/r/n/r/n
        handle->send_file_size = read_count;
#ifdef _DEBUG
        printf("response_headers_length:%d send_file_size:%d\n", handle->response_headers_length, handle->send_file_size);
        fflush(stdout);
#endif
        handle->write_ptr = handle->write_buf = dynamic_file_buf;
    }

    return 0; //仅起占位作用
}

int run_cgi_post(httphandle* handle, char* file_path)
{
    pid_t pid;
    char *dynamic_doc_index, *dynamic_file_buf, content_length[32];
    int status, pipe_send_to_child[2], pipe_recv_from_child[2];
    pipe(pipe_send_to_child);
    pipe(pipe_recv_from_child);
    dynamic_file_buf = malloc(WRITE_BUF_SIZE);
    if (dynamic_file_buf == NULL) {
        perror("allocate dynamic_file_buf failed! NEED_DISCONNECT");
        return NEED_DISCONNECT;
    }
    if ((pid = fork()) < 0) {
        perror("fork() error! NEED_DISCONNECT");
        return NEED_DISCONNECT;
    } else if (pid == 0) {
        Close(pipe_recv_from_child[0]);
        dup2(pipe_send_to_child[0], STDIN_FILENO);
        dup2(pipe_recv_from_child[1], STDOUT_FILENO);
        setenv("REQUEST_METHOD", "POST", 1);
        sprintf(content_length, "%d", handle->post_content_length);
        setenv("CONTENT_LENGTH", content_length, 1);
        setenv("CONTENT_TYPE", "application/x-www-form-urlencoded", 1);
        execle(file_path, file_path, NULL, __environ);
    } else {
        int count, left = handle->post_content_length, has_written = 0, read_count = 0;

        Close(pipe_send_to_child[0]);
        Close(pipe_recv_from_child[1]);

        while (left) {
            count = writen(pipe_send_to_child[1], handle->read_ptr + has_written, handle->post_content_length - has_written);
            has_written += count;
            left -= count;
        }

        Close(pipe_send_to_child[1]);
        // do{
        //     count=Read(pipe_recv_from_child[0], dynamic_file_buf, WRITE_BUF_SIZE);
        //     read_count+=count;
        // }while(count);
        read_count = Read(pipe_recv_from_child[0], dynamic_file_buf, WRITE_BUF_SIZE);
        wait(&status);
        dynamic_file_buf[read_count] = '\0';
        Close(pipe_recv_from_child[0]);

#ifdef _DEBUG
        printf("%d bytes read from cgi:%.*s\n", read_count, 200, dynamic_file_buf);
        if (WIFEXITED(status)) {
            printf("cgi exited with %d\n", WEXITSTATUS(status));
        }
        fflush(stdout);
#endif
        if (WIFSIGNALED(status)) {
            printf("cgi exited with error: %d\n", WTERMSIG(status));
            fflush(stdout);
            return NEED_DISCONNECT;
        }

        dynamic_doc_index = strstr(dynamic_file_buf, "\r\n\r\n");
        if (!dynamic_doc_index) {
            perror("POST. Bad dynamic_doc! NEED_DISCONNECT");
            handle->static_dynamic = STATIC_FILE;
            return NEED_DISCONNECT;
        }
        handle->response_headers_length = dynamic_doc_index + 4 - dynamic_file_buf; //cgi程序发送请求首部的大小 要算上/r/n/r/n
        handle->send_file_size = read_count;
#ifdef _DEBUG
        printf("response_headers_length:%d send_file_size:%d\n", handle->response_headers_length, handle->send_file_size);
        fflush(stdout);
#endif
        handle->write_ptr = handle->write_buf = dynamic_file_buf;
    }

    return 0; //仅起占位作用
}