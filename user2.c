/*
===================================== 
Assignment 4 Submission 
Name: Satkuri Sumith Chandra 
Roll number: 22CS10067  
===================================== 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include "ksocket.h"

int main(int argc,char* argv[]) {
    char filename[100];
    if(argc>1){
        strcpy(filename,argv[1]);
    }
    else{
        perror("Filename not specified\n");
        exit(1);
    }       
    int sockfd_ind = k_socket(AF_INET, 0, 0);
    if(sockfd_ind<0){
        perror("Failed to create socket");
        exit(1);
    }
    struct sockaddr_in local_addr,remote_addr;
    char IP_1[100],IP_2[100];
    int Port_1,Port_2;
    
    strcpy(IP_1,"127.0.0.1");
    strcpy(IP_2,"127.0.0.1");
    Port_1 = 3002;
    Port_2 = 2002;
    // Set remot0 IP_2 and Port_2
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(Port_2);
    inet_pton(AF_INET, IP_2, &remote_addr.sin_addr);

    int b= k_bind(sockfd_ind,IP_1,Port_1,IP_2,Port_2,&remote_addr);
    if(b<0){
        perror("Binding failed");
        k_close(sockfd_ind);
        exit(1);
    }
    

    FILE *file = fopen(filename, "w");
    if(!file){
        perror("Failed to open file");
        k_close(sockfd_ind);
        exit(1);
    }
    char buffer[MSG_SIZE];

    while (1) {
        memset(buffer,'\0',MSG_SIZE*sizeof(char));
        int n = k_recvfrom(sockfd_ind, buffer, MSG_SIZE, (struct sockaddr*)&remote_addr, sizeof(remote_addr));
        while(n < 0 && errno == ENOMESSAGE){ // ENOMSG instead of ENOMESSAGE
            usleep(500000);
            n = k_recvfrom(sockfd_ind, buffer, MSG_SIZE,(struct sockaddr*)&remote_addr, sizeof(remote_addr));
        }

        if(n < 0){
            perror("Socket closed");
            break;
        }
        // printf("%ld came here %s\n",time(NULL),buffer);
        fprintf(file,"%s",buffer);
        fflush(file);
    }

    fclose(file);
    k_close(sockfd_ind);
    printf("File reception completed.\n");
    return 0;
}
//(struct sockaddr*)&remote_addr, sizeof(remote_addr)