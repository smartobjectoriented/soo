#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/wait.h>
#include  <pthread.h>
#include <netinet/tcp.h>

#include "rcv_me.h"
#include "send_me.h"

#define IPV4_SRV "172.17.0.1"
#define PORT 7070

#ifndef FILE_SIZE 
#define FILE_SIZE (1 << 30) /* 1 GB */
#endif



int main(int argc, char *argv[])
{

    struct sockaddr_in addr_srv;
    pthread_t rcv_id;
    int sock_srv;
    int i = 5;
    int j = 10;
    struct timeval stopSend,stopRcv, start;


    /* arg parsing */
    if (argc != 1) {
        fprintf(stderr, "Usage:  %s\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    memset(&addr_srv, 0, sizeof(addr_srv));
    addr_srv.sin_family = AF_INET;
    addr_srv.sin_addr.s_addr = inet_addr(IPV4_SRV);
    addr_srv.sin_port = htons(PORT);

     /* socket init */
    if ((sock_srv = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        perror("Error creating client socket");
        exit(EXIT_FAILURE);
    }

    if (connect(sock_srv, (struct sockaddr *) &addr_srv, sizeof(addr_srv)) < 0) {
        perror("connect() error");
        exit(EXIT_FAILURE);
    }

   


    //start rcv thread
    pthread_create(&rcv_id, NULL, start_rcv, (void*)&sock_srv);

    //attendre les autre proc ai démaré leur thread rcv
    sleep(2);

    gettimeofday(&start, NULL);
    while(j--){
         while(i--){
            start_send((void*)&sock_srv);
        }
        i = 5;
        //usleep(300000U);

    }
    gettimeofday(&stopSend, NULL);   
    printf("time to send %uKB : %lu us\n",(FILE_SIZE/1024) * 50 ,((stopSend.tv_sec - start.tv_sec) * 1000000 + stopSend.tv_usec - start.tv_usec)); 



    pthread_join(rcv_id, NULL);
    gettimeofday(&stopRcv, NULL);   
    printf("time to rcv %uKB : %lu us\n",(FILE_SIZE/1024) * 50 *3, ((stopRcv.tv_sec - start.tv_sec) * 1000000 + stopRcv.tv_usec - start.tv_usec)); 
    close(sock_srv);
  



   


    return EXIT_SUCCESS;
}
