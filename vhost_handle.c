#include "vhost_handle.h"

//!!!!
extern vhost_list v_list;

int load_vhost_conf()
{
    int cwd_len;
    char cwd[WWW_ROOT_LENGTH], line_buf[HOST_LENGTH+WWW_ROOT_LENGTH];
    v_list.vhosts_num = 0;
    FILE* vhost_conf;

    getcwd(cwd, WWW_ROOT_LENGTH);
    cwd_len = strlen(cwd);
    strcpy(&cwd[cwd_len], "/vhost.conf");
#ifdef _DEBUG
    printf("vhost.conf path:%s\n", cwd);
#endif
    if ((vhost_conf = fopen(cwd, "r")) == NULL) {
        perror_exit("vhost.conf doesn't exsit!");
    }
    while (fgets(line_buf, HOST_LENGTH+WWW_ROOT_LENGTH, vhost_conf)) {

        if (line_buf[0] == '#' || line_buf[0] == '\n')
            continue;

        if (sscanf(line_buf, "%s %s", v_list.vhosts[v_list.vhosts_num].host_name, v_list.vhosts[v_list.vhosts_num].www_root) < 2) {
            printf("Bad vhost.conf:%s\n",line_buf);
            continue;
        }

#ifdef _DEBUG
        printf("vhost %d: %s %s\n", v_list.vhosts_num, v_list.vhosts[v_list.vhosts_num].host_name, v_list.vhosts[v_list.vhosts_num].www_root);
#endif
        v_list.vhosts_num++;
    }
    fclose(vhost_conf);
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
