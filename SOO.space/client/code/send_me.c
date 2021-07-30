#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define BUF_SIZE 65536

#ifndef SRV_FILE
#define SRV_FILE "srv_data.bin"
#endif



#ifndef FILE_SIZE 
#define FILE_SIZE (1 << 30) /* 1 GB */
#endif

 #define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

static void client_send(int sock, int file)
{
    ssize_t ret;
    ssize_t send_data;
    ssize_t send_data_tot;
    unsigned bytsToRead = FILE_SIZE;
    u_int8_t bufread[BUF_SIZE];
    unsigned int debugSend = 0;

    if (send(sock, &bytsToRead, sizeof(unsigned),0) != sizeof(unsigned)) {
        perror("send() end error");
        exit(EXIT_FAILURE);
    }

   
    while (bytsToRead) {
        ret = read(file, &bufread, min(sizeof(bufread),bytsToRead));

        if (ret < 0) {
            perror("listen() error");
            exit(EXIT_FAILURE);
        }

        send_data_tot = 0;

       while(send_data_tot < ret){
            if ((send_data = send(sock, &bufread[send_data_tot], min(sizeof(bufread),bytsToRead) - send_data_tot, 0)) < 0) {
                perror("send() error");
                exit(EXIT_FAILURE);
            }

           send_data_tot += send_data;
        }

        bytsToRead -= send_data_tot;
        debugSend += send_data_tot;

    }

    printf("[%s] sent %d bytes\n", __func__, debugSend + 4);
}

void* start_send(void* sock)
{
    int file;
    int sock_srv = *((int*) sock);


    /* file init */
    file = open(SRV_FILE, O_RDONLY, 0);

    if (file < 0) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    client_send(sock_srv, file);



    close(file);
    return 0;
}
