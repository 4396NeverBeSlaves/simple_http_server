#ifndef __VHOST_HANDLE_H__
#define __VHOST_HANDLE_H__

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include "wrap.h"

#define HOST_LENGTH 256
#define WWW_ROOT_LENGTH 256

typedef struct vhost {
    char host_name[HOST_LENGTH];
    char www_root[WWW_ROOT_LENGTH];
} vhost;

typedef struct vhost_list {
    vhost vhosts[32];
    int vhosts_num;
} vhost_list;

int load_vhost_conf();
int  get_vhost_id(char* host);

#endif
