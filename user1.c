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

    srand(time(NULL));
    
    strcpy(IP_1,"127.0.0.1");
    strcpy(IP_2,"127.0.0.1");
    Port_1 = 2002;
    Port_2 = 3002;
    
    // Set remote IP_2 and Port_2
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(Port_2);
    inet_pton(AF_INET, IP_2, &remote_addr.sin_addr);
    // sleep(10);
    printf("going into bind\n");

    int b = k_bind(sockfd_ind,IP_1,Port_1,IP_2,Port_2,&remote_addr);
    if(b<0){
        perror("Binding failed");
        k_close(sockfd_ind);
        exit(1);
    }
    printf("binded\n");
    FILE *file = fopen(filename, "rb");
    if(!file){
        perror("Failed to open file");
        k_close(sockfd_ind);
        exit(1);
    }
    char buffer[MSG_SIZE];
    int len;
    memset(buffer,'\0',MSG_SIZE*sizeof(char));
    while(len = fread(buffer, 1,400, file) > 0) {
        int status = k_sendto(sockfd_ind, buffer, len, (struct sockaddr *)&remote_addr, sizeof(remote_addr));
        while(status < 0 && errno == ENOSPACE){
            printf("NO space, sending again after 2 seconds\n");
            sleep(2);
            status = k_sendto(sockfd_ind, buffer, len, (struct sockaddr *)&remote_addr, sizeof(remote_addr));
            
        }
        if(status < 0){
            perror("Socket transmission error");
            break;
        }
        memset(buffer,'\0',MSG_SIZE*sizeof(char));
    }
    printf("Sending file completed.\n");

    fclose(file);
    k_close(sockfd_ind);
    printf("File transmission completed.\n");
    return 0;
}
