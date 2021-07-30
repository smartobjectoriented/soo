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

#ifndef CLIENT_FILE
#define CLIENT_FILE "client_data.bin"
#endif

#define ENDME "ENDME"
#define SIZE_END 6


#define BUF_SIZE 1048576 //1MB


#ifndef FILE_SIZE 
#define FILE_SIZE 8192 /* 1 GB */
#endif

#define min(a,b) \
({ __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a < _b ? _a : _b; })


static void client_recv(const int sock, const int file)
{
    ssize_t retR;
    ssize_t retW;
    static char buf[BUF_SIZE];

    
    ssize_t byte_write = 0;
    unsigned byte_to_rcv = 0;

     /* receive ME size from server */
    if ((retR = recv(sock, &byte_to_rcv,sizeof(unsigned), 0)) != sizeof(unsigned)) {
        perror("recv() error");
        exit(EXIT_FAILURE);
    }


    while (byte_to_rcv) {

        /* receive ME from server */
        if ((retR = recv(sock, &buf, min(BUF_SIZE,byte_to_rcv), 0)) < 0) {
            perror("recv() error");
            exit(EXIT_FAILURE);
        }


       retW = write(file, &buf, retR);
     
        if (retW < 0) {
            perror("write() error");
            exit(EXIT_FAILURE);
        }

        if (retW != retR) {
            fprintf(stderr, "[%s] Error: %ld bytes written (expected %ld)", __func__, retW, retR);
            exit(EXIT_FAILURE);
        }
        byte_write += retW;
        byte_to_rcv -= retW;
       
    }

    printf("[%s] written %ld bytes\n", __func__, byte_write);
}

void *start_rcv(void* sock)
{
    int file;
    int sock_srv = *((int*) sock);
    char file_name[30];
    int i;

    for(i =0; i< 150; i++){

        sprintf(file_name,"%s%d",CLIENT_FILE,i);

        /* file init */
        file = open(file_name, O_RDWR|O_CREAT, 0666);
        if (file < 0) {
            perror("Error opening file");
            exit(EXIT_FAILURE);
        }
        /* connected */
        client_recv(sock_srv, file);
        close(file);

    }
  
    


    return 0;
}
