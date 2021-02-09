/**
 * Copyright (C) 2020 Julien Quartier <julien.quartier@heig-vd.ch>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
#include <string.h>
#include <unistd.h>
#include <syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>
#include <dirent.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "ftp.h"

int serv_ip;

struct cmds ftp_cmd[] = {
        { .ident = "USER", .op = op_user, .login = 0 },
        { .ident = "PASS", .op = op_pass, .login = 0 },
        { .ident = "SYST", .op = op_syst, .login = 1 },
        { .ident = "PASV", .op = op_pasv, .login = 1 },
        // { .ident = "FEAT", .op = op_feat, .login = 1 },
        { .ident = "RETR", .op = op_retr, .login = 1 },
        { .ident = "STOR", .op = op_stor, .login = 1 },
        { .ident = "CWD", .op = op_cwd, .login = 1 },
        { .ident = "PWD", .op = op_pwd, .login = 1 },
        { .ident = "LIST", .op = op_list, .login = 1 },
        { .ident = "TYPE", .op = op_type, .login = 1 },
        { .ident = "DELE", .op = op_dele, .login = 1 },
        { .ident = "SIZE", .op = op_size, .login = 1 },
        { .ident = NULL, .op = NULL, .login = 0 } };


void joinpath(char *dst, const char *pth1, const char *pth2)
{
        char *rm, *fn;
        int l;

        if(pth1 == NULL && pth2 == NULL) {
                strcpy(dst, "");
        }
        else if(pth2 == NULL || strlen(pth2) == 0) {
                strcpy(dst, pth1);
        }
        else if(pth1 == NULL || strlen(pth1) == 0) {
                strcpy(dst, pth2);
        }
        else {
                char directory_separator[] = "/";
                const char *last_char = pth1;
                while(*last_char != '\0')
                        last_char++;
                int append_directory_separator = 0;
                if(strcmp(last_char, directory_separator) != 0) {
                        append_directory_separator = 1;
                }
                strcpy(dst, pth1);
                if(append_directory_separator)
                        strcat(dst, directory_separator);
                strcat(dst, pth2);
        }

        while((rm = strstr (dst, "/../")) != NULL) {
                for(fn = (rm - 1); fn >= dst; fn--) {
                        if(*fn == '/') {
                                l = strlen(rm + 4);
                                memcpy(fn + 1, rm + 4, l);
                                *(fn + l + 1) = 0;
                                break;
                        }
                }
        }
}

int close_data_socket(connection_t* user){
        if(user->fd_data_main >= 0)
                close(user->fd_data_main);

        if(user->fd_data >= 0)
                close(user->fd_data);

        user->fd_data_main = -1;
        user->fd_data = -1;

        return FTP_OK;
}

int create_data_socket(connection_t* user){
        struct sockaddr_in data_addr;
        int res;

        data_addr.sin_family = AF_INET;
        data_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        data_addr.sin_port = user->port;

        close_data_socket(user);

        user->fd_data_main = socket(AF_INET, SOCK_STREAM, 0);

        if((res = bind(user->fd_data_main, (struct sockaddr *) &data_addr, sizeof(data_addr))) < 0){
                printf("Impossible to bind data %d\n", res);
                return FTP_ERR;
        }

        if((res = listen(user->fd_data_main, 10)) < 0){
                printf("Impossible to listen data %d\n", res);
                return FTP_ERR;
        }

        return FTP_OK;
}

int get_data_fd(connection_t* user){
        if((user->fd_data = accept(user->fd_data_main, NULL, NULL)) < 0){
                printf("Impossible to accept  data\n");
                return FTP_ERR;
        }
        return FTP_OK;
}


int send_cmd(connection_t* user, int code, char* data){
        int len = snprintf(user->buff_tx, BUFF_SIZE, "%d %s\r\n", code, data);
        write(user->fd, user->buff_tx, len);

        return 0;
}

int op_user(connection_t* user, char* param){
        strcpy(user->user, param);
        printf("USER: '%s'\n", user->user);
        send_cmd(user, 331, "Please specify the password.");
        user->anonymous = !strcmp("anonymous", user->user);

        return FTP_OK;
}

int op_pass(connection_t* user, char* param){
        if(!user->anonymous){
                send_cmd(user, 430, "Authentication failed.");
                return FTP_ERR;
        }

        user->connected = 1;
        send_cmd(user, 230, "Login successful.");

        return FTP_OK;
}

int op_syst(connection_t* user, char* param){
        send_cmd(user, 215, "UNIX Type: L8");
        return FTP_OK;
}

int op_pasv(connection_t* user, char* param){
        char str_buff[255];
        user->port = htons((rand() % 3000) + 10000 + user->fd);

        printf("\n\nPORT %d\n\n", user->port);
        snprintf(str_buff, 255, "Entering Passive Mode (%d,%d,%d,%d,%d,%d)",
                 (serv_ip >> 0) & 0xFF, (serv_ip >> 8) & 0xFF, (serv_ip >> 16) & 0xFF, (serv_ip >> 24) & 0xFF,
                 (user->port >> 0) & 0xFF, (user->port >> 8) & 0xFF);

        create_data_socket(user);

        send_cmd(user, 227, str_buff);

        return FTP_OK;
}

int op_feat(connection_t* user, char* param){
        return FTP_ERR;
}

int op_retr(connection_t* user, char* param){
        int amount;
        long file_size = 0;
        FILE* f;
        char str_buff[255];

        if(get_data_fd(user) != FTP_OK){
                //TODO ERR
                goto close_conn;
        }

        joinpath(str_buff, user->dir, param);

        f = fopen(str_buff, "r");
        if (f == NULL) {
                printf("Unable to open %s\n", param);
                return 1;
        }

        fseek(f, 0, SEEK_END);
        file_size = ftell(f);
        fseek(f, 0, SEEK_SET);

        snprintf(str_buff, 255, "Opening BINARY mode data connection for %s (%d bytes)", param, file_size);
        send_cmd(user, 150, str_buff);

        while ((amount = fread(user->buff_data_tx, sizeof(char), BUFF_SIZE, f)) > 0) {
                printf("Amount %d\n", amount);
                printf("POS %d\n", ftell(f));
                printf("Write\n");
                write(user->fd_data, user->buff_data_tx, amount);
                printf("READ\n");
        }

        fclose(f);

        send_cmd(user, 226, "Transfer complete.");

        close_data_socket(user);
        return FTP_OK;

        close_conn:
        close_data_socket(user);
        return FTP_ERR;
}

int op_stor(connection_t* user, char* param){
        char str_buff[255];
        int amount;
        FILE* f;

        snprintf(str_buff, 255, "Opening data channel for file upload to server of \"%s\"", param);
        send_cmd(user, 150, str_buff);

        if(get_data_fd(user) != FTP_OK){
                //TODO ERR
                goto stor_err;
        }

        joinpath(str_buff, user->dir, param);

        f = fopen(str_buff, "w+");
        if (f == NULL) {
                printf("Unable to open %s\n", str_buff);
                goto stor_err;
        }

        while ((amount = read(user->fd_data, user->buff_data_rx, BUFF_SIZE)) > 0) {
                fwrite(user->buff_data_rx, sizeof(char), amount, f);
        }

        fclose(f);

        snprintf(str_buff, 255, "Successfully transferred \"%s\"", param);
        send_cmd(user, 226, str_buff);

        close_data_socket(user);
        return FTP_OK;

        stor_err:
        snprintf(str_buff, 555, "Successfully transferred \"%s\"", param);
        send_cmd(user, 226, str_buff);
        close_data_socket(user);
        return FTP_ERR;
}

int op_dele(connection_t* user, char* param){
        char str_buff[255];

        joinpath(str_buff, user->dir, param);

        if(!unlink(str_buff)){
                send_cmd(user, 250, "File deleted.");
                return FTP_OK;
        }

        send_cmd(user, 550, "The file can't be deleted.");
        return FTP_ERR;
}



int op_cwd(connection_t* user, char* param){
        char str_buff[255];
        printf("Userfolder  %s\n", param);

        if(param[0] == '/'){
                strcpy(user->dir, param);
                goto cwd_ok;
        }

        if(user->dir[0] == '/' && user->dir[1] == '\0'){
                strcpy(user->dir + 1, param);
                goto cwd_ok;
        }

        joinpath(str_buff, user->dir, param);

        strcpy(user->dir, str_buff);


        cwd_ok:
        printf("Userfolder %s\n", user->dir);

        send_cmd(user, 250, "Okay.");
        return FTP_OK;

        send_cmd(user, 550, "No such file or directory.");
        return FTP_ERR;
}

int op_pwd(connection_t* user, char* param){
        char str_buff[255];
        snprintf(str_buff, 255, "\"%s\" is the current directory", user->dir);
        send_cmd(user, 257, str_buff);

        return FTP_OK;
}

int op_list(connection_t* user, char* param){
        DIR *stream;
        struct dirent  *p_entry;
        int len;
        long size = 0;
        FILE* f;
        char str_buff[255];

        if(get_data_fd(user) != FTP_OK){
                //TODO ERR
                goto close_conn;
        }

        stream = opendir(user->dir);
        if (stream == NULL)
                goto close_conn;

        send_cmd(user, 150, "Here comes the directory listing.");

        while ((p_entry = readdir(stream)) != NULL){

                printf("LEN %d TYPE %d \n", p_entry->d_reclen, p_entry->d_type);

                if(p_entry->d_type == DT_DIR){ // Directory
                        len = snprintf(user->buff_data_tx, BUFF_SIZE, "drwxrwxrwx 1 root root 0 jan 1 00:00 %s\r\n", p_entry->d_name);
                        write(user->fd_data, user->buff_data_tx, len);

                } else if(p_entry->d_type == DT_REG) { // File
                        joinpath(str_buff, user->dir, p_entry->d_name);

                        /* File size */
                        f = fopen(str_buff, "r");
                        printf("FD = %d\n", f);
                        if(f <= 0)
                                continue;

                        fseek(f, 0, SEEK_END);
                        size = ftell(f);
                        fclose(f);


                        len = snprintf(user->buff_data_tx, BUFF_SIZE, "-rwxrwxrwx 1 root root %d jan 1 00:00 %s\r\n", size,  p_entry->d_name);
                        write(user->fd_data, user->buff_data_tx, len);
                }
        }

        send_cmd(user, 226, "Directory send OK.");

        close_conn:
        close_data_socket(user);
        return FTP_ERR;
}

int op_type(connection_t* user, char* param){
        if(param[0] == 'A' || param[0] == 'I'){
                user->type = param[0];
                send_cmd(user, 200, "Done");
        } else {
                send_cmd(user, 504, "Only type A (ASCII) and I (Image) are implemented");
        }
        return FTP_OK;
}

int op_size(connection_t* user, char* param){
        long file_size = 0;
        FILE* f;
        char str_buff[255];

        joinpath(str_buff, user->dir, param);

        f = fopen(str_buff, "r");
        if (f == NULL) {
                send_cmd(user, 213, "0");
                return FTP_OK;
        }

        fseek(f, 0, SEEK_END);
        file_size = ftell(f);
        fseek(f, 0, SEEK_SET);

        fclose(f);

        snprintf(str_buff, 255, "%d", file_size);
        send_cmd(user, 213, str_buff);
        return FTP_OK;
}

void get_serv_ip(){
        struct ifreq ifr;
        int s;

        ifr.ifr_addr.sa_family = AF_INET;
        strncpy(ifr.ifr_name, "vi0", 15);
        ifr.ifr_name[15] = 0;

        s = socket(AF_INET, SOCK_STREAM, 0);

        if(ioctl(s, SIOCGIFINDEX, &ifr) != 0) {
                printf("Interface '%s' not found\n", ifr.ifr_name);
                close(s);
                return;
        }

        if (ioctl(s, SIOCGIFADDR, &ifr) == 0) {
                serv_ip = ((struct sockaddr_in *) &ifr.ifr_ifru.ifru_addr)->sin_addr.s_addr;
        }
}

int handle_cmd(connection_t* user, char* buff){
        char *cmd = buff, *content, *end;
        int i = 0;

        if((end = strstr(buff, "\r\n")) != NULL)
                *end = '\0';

        if((content = strstr(buff, " ")) != NULL){
                *content = '\0';
                content++;
        }

        printf("CMD: (%s)\n", cmd);

        while(ftp_cmd[i].ident != NULL){
                if(!strncmp(ftp_cmd[i].ident, cmd, strlen(ftp_cmd[i].ident))){
                        if(ftp_cmd[i].login && !user->connected){
                                send_cmd(user, 530, "Please login with USER and PASS.");
                                return FTP_ERR;
                        }

                        printf("IDENT: (%s)  CMD: (%s)\n", ftp_cmd[i].ident, cmd);

                        printf("FNC: (%p)\n", ftp_cmd[i].op);

                        ftp_cmd[i].op(user, content);
                        return FTP_OK;
                }
                i++;
        }

        send_cmd(user, 502, "Unknown command.");

        return FTP_ERR;
}

void* user_thread(void* arg){
        int read_len;
        connection_t* user = arg;
        user->type = 'A';
        user->dir[0] = '/';
        user->dir[1] = '\0';
        user->connected = 0;
        user->fd_data = -1;
        user->fd_data_main = -1;

        printf("New client connected %d\n", user->fd);

        send_cmd(user, 220, "SO3 FTP server 0.1");

        while (user->fd > 0) {
                if ((read_len = read(user->fd, user->buff, BUFF_SIZE - 1)) > 0) {
                        user->buff[read_len] = 0;
                        //printf("The client said: %s", user->buff);
                        handle_cmd(user, user->buff);
                } else {
                        printf("closing...");
                        close_data_socket(user);
                        close(user->fd);
                        user->fd = -1;
                        printf("closed");

                }

        }

        return NULL;
}

connection_t user_connections[MAX_CONNECTIONS];
connection_t* find_available_connection(){
        int i = 0;
        while(i < MAX_CONNECTIONS){
                if(user_connections[i].fd < 0)
                        return user_connections + i;
                i++;
        }

        return NULL;
}

void init_connections(){
        int i = 0;
        while(i < MAX_CONNECTIONS){
                user_connections[i].type = 'A';
                user_connections[i].dir[0] = '/';
                user_connections[i].dir[1] = '\0';
                user_connections[i].connected = 0;
                user_connections[i].fd = -1;
                user_connections[i].fd_data = -1;
                user_connections[i].fd_data_main = -1;
                i++;
        }
}

int main(int argc, char **argv)
{
        int master, user_fd, err = 0;
        struct sockaddr_in srv_addr, client_addr;
        connection_t* user;
        /*struct timeval timeout;

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;*/

        init_connections();

        get_serv_ip();

        memset(&client_addr, 0, sizeof(client_addr));
        memset(&srv_addr, 0, sizeof(srv_addr));

        master = socket(AF_INET, SOCK_STREAM, 0);

        srv_addr.sin_family = AF_INET;
        srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        srv_addr.sin_port = htons(FTP_PORT_MASTER);

        //setsockopt(master, 0xfff, 0x1005, (const char *) &timeout, sizeof(struct timeval));
        //setsockopt(master, 0xfff, 0x1006, (const char *) &timeout, sizeof(struct timeval));


        if (setsockopt(master, 0xfff, 0x0008, &(int){1}, sizeof(int)) < 0)
                printf("keep alive failed");

        if((err = bind(master, (struct sockaddr *) &srv_addr, sizeof(srv_addr))) < 0){
                printf("Impossible to bind main %d\n", err);
                return -1;
        }

        if(listen(master, 100) < 0){
                printf("Impossible to listen main\n");
                return -1;
        }

        printf("\nWaiting for clients...\n");

        while(1){
                user_fd = accept(master, NULL, NULL);
                if(user_fd < 0){
                        printf("Error on accept\n");
                } else {
                        user = find_available_connection();

                        if(user != NULL){
                                user->fd = user_fd;
                                pthread_create(&user->main_th, NULL, user_thread, user);
                        } else {
                                printf("No connection available\n");
                        }

                }
                user_fd = -1;
        }

        return 0;
}