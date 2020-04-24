#include "epoll_operation.h"
#include "httphandle.h"
#include "wrap.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

char* default_index_file = "./index.html";

int main(int argc, char** argv)
{
    int epfd, current_fd, lfd, events_ready, action_code;
    int i, request_count = 0;
    struct epoll_event evts[MAXEVENTS];
    httphandle handles[MAXEVENTS];

    int port = 80;
    // char* dir = "./";

    if (argc == 2) {
        port = atoi(argv[1]);
        // dir = argv[2];
    } else
        printf("use: ./server <port>\n");

    printf("running at: port:%d\n", port);
    // printf("running at: port:%d dir: %s\n", port, dir);
    // chdir(dir);
    epfd = Epoll_create(100);
    lfd = init_listen_fd(port);
    // printf("lfd:%d\n",lfd);
    addfd(epfd, lfd);

    int epoll_wait_times = 0;
    while (1) {
#ifdef _DEBUG
        printf("epoll wait times:%d\n", ++epoll_wait_times);
        fflush(stdout);
#endif
        events_ready = Epoll_wait(epfd, evts, MAXEVENTS, -1);
#ifdef _DEBUG
        printf("events_ready:%d\n", events_ready);
        fflush(stdout);
#endif
        // if (events_ready == 0)
        //     continue;

        for (i = 0; i < events_ready; ++i) {

            current_fd = evts[i].data.fd;

            if (current_fd == lfd) {
#ifdef _DEBUG
                printf("client is coming!\n");
                fflush(stdout);
#endif
                int cli_count;
                cli_count = accept_clients(epfd, lfd, handles);
#ifdef _DEBUG
                printf("current loop:No.%d  client count:%d\n\n\n\n", i + 1, cli_count);
                fflush(stdout);
#endif

            } else if (evts[i].events & EPOLLIN) {
#ifdef _DEBUG
                printf("EPOLLIN current loop:No.%d cfd:%d request count:%d\n", i + 1, current_fd, ++request_count);
                fflush(stdout);
#endif
                action_code = do_read(current_fd, &handles[current_fd]);
                if (action_code == NEED_READ) {
#ifdef _DEBUG
                    printf("\n\n\n\naction_code == NEED_READ in evts[i].events & EPOLLIN\n\n\n");
                    fflush(stdout);
#endif
                    // modfd(epfd, current_fd, EPOLLIN);
                } else if (action_code == NEED_WRITE) {
#ifdef _DEBUG
                    printf("action_code == NEED_WRITE in evts[i].events & EPOLLIN\n");
                    fflush(stdout);
#endif
                    modfd(epfd, current_fd, EPOLLOUT);
                } else {
                    disconnect(epfd, &handles[current_fd]);
                }

            } else if (evts[i].events & EPOLLOUT) {
#ifdef _DEBUG
                printf("EPOLLOUT current loop:No.%d cfd:%d\n", i, current_fd);
#endif
                action_code = do_write(current_fd, &handles[current_fd]);
                if (action_code == NEED_READ) {
                    modfd(epfd, current_fd, EPOLLIN);
#ifdef _DEBUG
                    printf("\n\n\n\n\n\naction_code == NEED_READ in evts[i].events & EPOLLIN\n");
                    fflush(stdout);
#endif
                } else if (action_code == NEED_WRITE) {
                    modfd(epfd, current_fd, EPOLLOUT);
#ifdef _DEBUG
                    printf("action_code == NEED_WRITE in evts[i].events & EPOLLIN\n");
                    fflush(stdout);
#endif
                } else {
                    disconnect(epfd, &handles[current_fd]);
                }
            }
        }
    }

    Close(epfd);
    Close(lfd);
    return 0;
}
