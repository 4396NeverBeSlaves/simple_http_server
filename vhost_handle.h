#ifndef __VHOST_HANDLE_H__
#define __VHOST_HANDLE_H__

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

typedef struct vhost {
    char host_name[256];
    char www_root[256];
} vhost;

typedef struct vhost_list {
    vhost vhosts[32];
    int vhosts_num;
} vhost_list;

int read_vhost_conf();
char* get_vhost_root_path(char* host);

#endif
