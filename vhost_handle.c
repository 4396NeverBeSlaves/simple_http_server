#include "vhost_handle.h"

//!!!!
extern vhost_list v_list;

int load_vhost_conf()
{
    char path[WWW_ROOT_LENGTH], line_buf[HOST_LENGTH + WWW_ROOT_LENGTH];
    v_list.vhosts=malloc(sizeof(vhost)*32);
    v_list.vhosts_num = 0;
    FILE* vhost_conf;

    get_process_path(path);
    strcat(path, "/vhost.conf");
#ifdef _DEBUG
    printf("vhost.conf path:%s\n", path);
#endif
    if ((vhost_conf = fopen(path, "r")) == NULL) {
        perror_exit("vhost.conf doesn't exsit!");
    }
    while (fgets(line_buf, HOST_LENGTH + WWW_ROOT_LENGTH, vhost_conf)) {

        if (line_buf[0] == '#' || line_buf[0] == '\n')
            continue;

        if (sscanf(line_buf, "%s %s", v_list.vhosts[v_list.vhosts_num].host_name, v_list.vhosts[v_list.vhosts_num].www_root) < 2) {
            printf("Bad vhost.conf:%s\n", line_buf);
            continue;
        }

#ifdef _DEBUG
        printf("vhost %d: %s %s\n", v_list.vhosts_num, v_list.vhosts[v_list.vhosts_num].host_name, v_list.vhosts[v_list.vhosts_num].www_root);
#endif
        v_list.vhosts_num++;
    }
    fclose(vhost_conf);

    if(v_list.vhosts_num==0)
        perror_exit("0 virtual host found. Please configure vhost.conf!");
    return v_list.vhosts_num;
}

int get_vhost_id(char* host)
{
    int i;

    for (i = 0; i < v_list.vhosts_num; i++) {
        if (!strcmp(v_list.vhosts[i].host_name, host)) {
            return i;
        }
    }
    return -1;
}