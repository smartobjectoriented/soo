//
// Created by julien on 9/7/20.
//

#ifndef USR_FTP_H
#define USR_FTP_H

#define FTP_PORT_MASTER 21

#define FTP_ERR -1
#define FTP_OK 0

#define BUFF_SIZE 1024

#define MAX_CONNECTIONS 10

typedef struct {
        int fd, fd_data, fd_data_main;
        short port;
        pthread_t main_th, data_th;
        char dir[255];
        int anonymous;
        struct sockaddr_in addr;
        int connected;
        char type;
        char user[32];
        char buff[1025];
        char buff_tx[1025];
        char buff_data_tx[1025];
        char buff_data_rx[1025];
} connection_t;

typedef   int (*operation)(connection_t* user, char* param);

struct cmds {
        const char* ident;
        operation op;
        int login;
};

int handle_cmd(connection_t* user, char* buff);

int op_user(connection_t* user, char* param);
int op_pass(connection_t* user, char* param);
int op_syst(connection_t* user, char* param);
int op_pasv(connection_t* user, char* param);
int op_feat(connection_t* user, char* param);
int op_retr(connection_t* user, char* param);
int op_cwd(connection_t* user, char* param);
int op_pwd(connection_t* user, char* param);
int op_list(connection_t* user, char* param);
int op_type(connection_t* user, char* param);
int op_stor(connection_t* user, char* param);
int op_dele(connection_t* user, char* param);
int op_size(connection_t* user, char* param);



#endif //USR_FTP_H
