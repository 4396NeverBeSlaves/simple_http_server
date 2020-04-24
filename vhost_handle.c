#include "vhost_handle.h"

vhost_list v_list;

int read_vhost_conf()
{
    int cwd_len;
    char cwd[128], line_buf[512];
    v_list.vhosts_num = 0;
    FILE* vhost_conf;

    getcwd(cwd, 128);
    cwd_len = strlen(cwd);
    strcpy(&cwd[cwd_len], "/vhost.conf");
#ifdef _DEBUG
    printf("vhost.conf path:%s\n", cwd);
#endif
    if ((vhost_conf = fopen(cwd, "r")) == NULL) {
        perror("");exit(-1);
    }
    while (fgets(line_buf, 512, vhost_conf)) {
        ;
        if (line_buf[0] == '#' || line_buf[0] == '\n')
            continue;

        if (sscanf(line_buf, "%s %s", v_list.vhosts[v_list.vhosts_num].host_name, v_list.vhosts[v_list.vhosts_num].www_root) < 2) {
            perror("vhost.conf is wrong!");
            continue;
        }

#ifdef _DEBUG
        printf("vhost %d: %s %s\n", v_list.vhosts_num + 1, v_list.vhosts[v_list.vhosts_num].host_name, v_list.vhosts[v_list.vhosts_num].www_root);
#endif
        v_list.vhosts_num++;
    }
    fclose(vhost_conf);
    return v_list.vhosts_num;
}

char* get_vhost_root_path(char* host)
{
    int i;

    for (i = 0; i < v_list.vhosts_num; i++) {
        if (!strcmp(v_list.vhosts[i].host_name, host)) {
            return v_list.vhosts[i].www_root;
        }
    }
    return NULL;
}

/*int main()
{

    read_vhost_conf();

    return 0;
}*/
