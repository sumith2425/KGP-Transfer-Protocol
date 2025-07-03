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
#include <sys/ipc.h>
#include <sys/shm.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <pthread.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>

#define p 0.15
#define N 10
#define T 5
#define RWND_T 2
#define SHM_KEY 10067
#define MAX_SOCKETS 10
#define MSG_SIZE 512
#define WINDOW_SIZE 10
#define BUFFER_SIZE 10
#define MAX_SEQ_NO 256
#define ENOTBOUND 1001
#define ENOSPACE 1002
#define ENOMESSAGE 1003
#define ERRORNP 1004
#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))


typedef struct{
    int seq_no[WINDOW_SIZE];
    int base;
    int wind_size; //window size
}swnd;

typedef struct{
    int recieved[WINDOW_SIZE];
    int base;
    int exp_seq;
}rwnd;

typedef struct{
    int is_free;
    int binded;
    int pid;
    int sockfd;
    char* dest_IP;
    int dest_port;

    struct sockaddr_in dest_addr,my_addr;
    char send_buffer[BUFFER_SIZE][MSG_SIZE];
    char recieve_buffer[BUFFER_SIZE][MSG_SIZE];
    char recent_ack[600];
    time_t timestamp;
    time_t rwnd_time;
    int send_seq_no;
    int send_buffer_size;
    
    int recieve_buffer_size;

    int reciever_rwnd;
    swnd send_window;
    rwnd recieve_window;
    pthread_mutex_t mutex;

    int total_transmissions;
    int total_messages;
}ksocket;

extern ksocket* SM;
extern pthread_t send_thread_id, receive_thread_id;

void init_shared_memory();
void *receiver_function(void *);
void *sender_function(void *);
void bind_function();
void garbage_collector();
int k_socket(int domain,int type,int protocol);
int k_bind(int sockfd_ind,char src_ip[],int src_port,char dest_ip[],int dest_port,struct sockaddr_in* dest_addr);
int k_sendto(int sockfd_ind,char msg[],int msg_size,struct sockaddr* dest_addr,socklen_t dest_addr_len);
int k_recvfrom(int sockfd_ind,char msg[],int msg_size,struct sockaddr* src_addr,socklen_t src_addr_len);
int k_close(int sockfd_ind);
int dropmessage(float p1);