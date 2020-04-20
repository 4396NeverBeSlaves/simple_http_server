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
#include <stdlib.h>
#include <unistd.h>
#include "httphandle.h"
#include "epoll_operation.h"
#include "wrap.h"


char *default_index_file="./index.html";

int main(int argc, char** argv)
{
    int epfd, current_fd,lfd, events_ready, action_code;
    int i,request_count=0;
    struct epoll_event evts[MAXEVENTS];
    httphandle handles[MAXEVENTS];

    int port=80;
    char *dir="./";

    if (argc == 3){
        port=atoi(argv[1]);
        dir=argv[2];
    }else
        printf("use: ./server <port> <www-root>\n");
    
    printf("running at: port:%d dir: %s\n",port,dir);
    chdir(dir);
    epfd = Epoll_create(100);
    lfd = init_listen_fd(port);
    // printf("lfd:%d\n",lfd);
    addfd(epfd, lfd);

    while (1) {
        events_ready = Epoll_wait(epfd, evts, MAXEVENTS, -1);
        if (events_ready == 0)
            continue;

        for (i = 0; i < events_ready; i++) {

            current_fd=evts[i].data.fd;

            if (current_fd == lfd) {

                accept_client(epfd, lfd, handles);

            } else if (evts[i].events & EPOLLIN) {
                #ifdef _DEBUG
                printf("request count:%d\n",++request_count);
                #endif
                action_code = do_read(current_fd,&handles[current_fd]);
                if (action_code == NEED_WRITE) {
                    modfd(epfd, current_fd, EPOLLOUT);
                } else {
                    disconnect(epfd,&handles[current_fd]);
                }

            } else if (evts[i].events & EPOLLOUT) {

                action_code = do_write(current_fd,&handles[current_fd]);
                if (action_code == NEED_READ) {
                    modfd(epfd, current_fd, EPOLLIN);
                }else if(action_code == NEED_WRITE){
                    modfd(epfd, current_fd, EPOLLOUT);
                }else {
                    disconnect(epfd,&handles[current_fd]);
                }
                
            }
        }
    }

    Close(epfd);
    Close(lfd);
    return 0;
}
