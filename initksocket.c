/*
===================================== 
Assignment 4 Submission 
Name: Satkuri Sumith Chandra 
Roll number: 22CS10067 
===================================== 
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include "ksocket.h"

#define DATA 0
#define ACK 1
extern ksocket* SM;

void *receiver_function(void *) {
    fd_set read_fds;
    struct timeval timeout;
    char buffer[600];
    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);

    while (1) {
        if (shmget(SHM_KEY, 0, 0) == -1){
            printf("Shared memory removed. Stopping reciever thread.\n");
            pthread_exit(NULL);
        }
        bind_function();
        FD_ZERO(&read_fds);
        int max_fd = -1;

        for (int i = 0; i < MAX_SOCKETS; i++) {
            pthread_mutex_lock(&SM[i].mutex);
            if (!SM[i].is_free&&SM[i].binded==1) {
                FD_SET(SM[i].sockfd, &read_fds);
                if (SM[i].sockfd > max_fd) max_fd = SM[i].sockfd;
            }
            pthread_mutex_unlock(&SM[i].mutex);
        }

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
        if (activity < 0){
            perror("select failed");
            continue;
        } 

        for (int i = 0; i < MAX_SOCKETS; i++) {
            pthread_mutex_lock(&SM[i].mutex);
            if (!SM[i].is_free && FD_ISSET(SM[i].sockfd, &read_fds)) {
                int sockfd = SM[i].sockfd;
                int recv_len=0;
                pthread_mutex_unlock(&SM[i].mutex);
                recv_len = recvfrom(sockfd, buffer, sizeof(buffer), 0,(struct sockaddr *)&sender_addr, &addr_len);
                
                pthread_mutex_lock(&SM[i].mutex);
                if(recv_len<0){
                    if(errno == EAGAIN || errno == EWOULDBLOCK){
                        pthread_mutex_unlock(&SM[i].mutex);
                        printf("here comes error\n");
                        continue;
                    }
                    perror("recvfrom error");
                    pthread_mutex_unlock(&SM[i].mutex);
                    continue;
                }
                if(recv_len > 0){
                    //extract the message
                    char msg[MSG_SIZE];
                    int seq_no, flag, var; //var is msg len if data packet and rwnd if ack packet
                    sscanf(buffer, "%d %d %d", &seq_no, &flag, &var);
                    if(dropmessage(p)){
                        pthread_mutex_unlock(&SM[i].mutex);
                        printf("\nDropped Seq no: %d, Type: %s\n\n",seq_no,(flag==DATA)?"DATA":"ACK");
                        fflush(stdout);
                        continue;
                    }
                    //process the message
                    if(flag==ACK){
                        printf("Received ACK for Seq no: %d,rwnd: %d\n", seq_no,var);
                        fflush(stdout);
                        int base=SM[i].send_window.base;
                        //if seq no is in window
                        int temp = (seq_no-base+1+MAX_SEQ_NO*10)%MAX_SEQ_NO;
                        int size = SM[i].send_window.wind_size;
                        if(temp<=size){
                            int len=strlen(SM[i].send_buffer[(base+temp-1)%BUFFER_SIZE]);
                            sprintf(SM[i].recent_ack,"%d %d %d %s",(base+temp-1)%MAX_SEQ_NO,DATA,len,SM[i].send_buffer[(base+temp-1)%BUFFER_SIZE]);
                            for(int j=0;j<temp;j++){
                                memset(SM[i].send_buffer[(base+j)%BUFFER_SIZE],'\0',MSG_SIZE*sizeof(char));
                            }
                            SM[i].send_window.base=(SM[i].send_window.base+temp);
                            SM[i].send_window.wind_size-=temp;
                            SM[i].send_buffer_size-=temp;
                            SM[i].reciever_rwnd = var;
                            if(var==0){
                                SM[i].rwnd_time=time(NULL);
                            }
                            if(SM[i].send_window.wind_size==0){
                                SM[i].timestamp=0;
                            }
                        }
                        else if(temp>(MAX_SEQ_NO-2*BUFFER_SIZE)){  //if seq no is not in window, i.e, duplicate
                            SM[i].reciever_rwnd = var;
                            if(var==0){
                                SM[i].rwnd_time=time(NULL);
                            }
                        }
                    }
                    else if(flag==DATA){
                        int offset = snprintf(NULL, 0, "%d %d %d ", seq_no, flag, var);  
                        const char *msg_start = buffer + offset;
                        memset(msg,'\0',sizeof(msg));
                        memcpy(msg, msg_start, var);
                        msg[var] = '\0';
                        int exp_seq = (SM[i].recieve_window.exp_seq);
                        int temp = (seq_no-exp_seq+10*MAX_SEQ_NO)%MAX_SEQ_NO;
                        int actual_seq_no = exp_seq+temp;
                        int rwnd_size = BUFFER_SIZE-SM[i].recieve_buffer_size;
                        if(temp<=rwnd_size){
                            if(SM[i].recieve_window.recieved[actual_seq_no%BUFFER_SIZE]==0){
                                SM[i].recieve_window.recieved[actual_seq_no%BUFFER_SIZE]=1;
                                strcpy(SM[i].recieve_buffer[actual_seq_no%BUFFER_SIZE],msg);
                                printf("Recieved DATA for Seq no: %d\n",actual_seq_no);
                                fflush(stdout);
                            }
                            int j=0;
                            for(j=0;j<rwnd_size;j++){
                                if(SM[i].recieve_window.recieved[(exp_seq+j)%BUFFER_SIZE]){
                                    SM[i].recieve_window.exp_seq = (SM[i].recieve_window.exp_seq+1);
                                }
                                else{
                                    break;
                                }
                            }
                            SM[i].recieve_buffer_size+=j;
                            rwnd_size-=j;
                            //send ack
                            if(j>0){
                                char ack[600];
                                sprintf(ack, "%d %d %d %s", (exp_seq+j-1+MAX_SEQ_NO)%MAX_SEQ_NO, ACK, rwnd_size,"ACK"); //ADD rwnd,
                                sendto(SM[i].sockfd, ack, strlen(ack), 0,(struct sockaddr *)&SM[i].dest_addr, sizeof(SM[i].dest_addr));
                                printf("Sent ACK Seq no: %d, Ack:%s\n",(exp_seq+j-1+256)%MAX_SEQ_NO,ack);
                                fflush(stdout);
                            }
                        }
                        else if(temp>(MAX_SEQ_NO-2*BUFFER_SIZE)){ //duplicate packet
                            //send ack
                            rwnd_size = BUFFER_SIZE-SM[i].recieve_buffer_size;
                            char ack[600];
                            sprintf(ack, "%d %d %d %s", (exp_seq-1+MAX_SEQ_NO)%MAX_SEQ_NO, ACK, rwnd_size,"ACK"); //ADD rwnd,
                            printf("Sent ACK Seq no: %d, Ack:%s\n",(exp_seq-1+256)%MAX_SEQ_NO,ack);
                            fflush(stdout);
                            sendto(SM[i].sockfd, ack, strlen(ack), 0,(struct sockaddr *)&SM[i].dest_addr, sizeof(SM[i].dest_addr));
                        }
                    }
                }
            }
            pthread_mutex_unlock(&SM[i].mutex);
        }
    }
}

void *sender_function(void *) {
    while (1) {
        sleep(T / 2);
        if(shmget(SHM_KEY,0,0)==-1){
            perror("Shared memory stopped");
            pthread_exit(NULL);
        }
        garbage_collector();
        for (int i = 0; i < MAX_SOCKETS; i++) {
            pthread_mutex_lock(&SM[i].mutex);
            if (SM[i].is_free) {
                pthread_mutex_unlock(&SM[i].mutex);
                continue;
            }
            //retransmitting messages
            if(SM[i].timestamp!=0 && (time(NULL)-SM[i].timestamp)>=T){
                for(int j=0;j<SM[i].send_window.wind_size;j++){
                    int base = SM[i].send_window.base;
                    char message[600];
                    int len = strlen(SM[i].send_buffer[(base)%WINDOW_SIZE]);
                    sprintf(message, "%d %d %d %s", (SM[i].send_window.seq_no[(base+j)%WINDOW_SIZE])%MAX_SEQ_NO, DATA, len,SM[i].send_buffer[(base+j)%WINDOW_SIZE]);
                    sendto(SM[i].sockfd, message,strlen(message), 0, (struct sockaddr *)&SM[i].dest_addr,sizeof(SM[i].dest_addr));
                    printf("Retransmitting Packet\n");
                    SM[i].total_transmissions++;
                }
                SM[i].timestamp = time(NULL);
            }
            int cnt = min(SM[i].send_buffer_size-SM[i].send_window.wind_size,SM[i].reciever_rwnd-SM[i].send_window.wind_size);
            if(cnt<0)cnt=0;
            //if rwnd was zero for RWND_T secs send a prev acked packet
            if(SM[i].reciever_rwnd==0&&SM[i].send_buffer_size>0){
                if(time(NULL)-SM[i].rwnd_time >RWND_T){
                    sendto(SM[i].sockfd,SM[i].recent_ack,strlen(SM[i].recent_ack), 0, (struct sockaddr *)&SM[i].dest_addr,sizeof(SM[i].dest_addr));
                    printf("Sent after rwnd wait \n");
                    SM[i].rwnd_time = time(NULL);
                    SM[i].total_transmissions++;
                }
            }
            //sending new messages
            for(int j=0;j<cnt;j++){
                time_t current_time = time(NULL);
                int base = SM[i].send_window.base;
                char message[600];
                int len = strlen(SM[i].send_buffer[(base+SM[i].send_window.wind_size+j)%WINDOW_SIZE]);
                sprintf(message, "%d %d %d %s", (base+SM[i].send_window.wind_size+j)%MAX_SEQ_NO, DATA, len,SM[i].send_buffer[(base+SM[i].send_window.wind_size+j)%WINDOW_SIZE]);
                sendto(SM[i].sockfd,message,strlen(message), 0, (struct sockaddr *)&SM[i].dest_addr,sizeof(SM[i].dest_addr));
                SM[i].send_window.seq_no[(base+SM[i].send_window.wind_size+j)%WINDOW_SIZE] = (base+SM[i].send_window.wind_size+j);
                printf("Sent Packet Seq no: %d\n",(base+SM[i].send_window.wind_size+j)%MAX_SEQ_NO);
                if(SM[i].timestamp==0){
                    SM[i].timestamp = time(NULL);
                }
                SM[i].total_transmissions++;
            }
            fflush(stdout);
            SM[i].send_window.wind_size+=cnt;
            pthread_mutex_unlock(&SM[i].mutex);
        }
    }
}

void bind_function(){
    for(int i=0;i<MAX_SOCKETS;i++){
        pthread_mutex_lock(&SM[i].mutex);
        if(SM[i].binded==0&& !SM[i].is_free){
            if(bind(SM[i].sockfd,(struct sockaddr*)&SM[i].my_addr,sizeof(SM[i].my_addr))==0){
                printf("is binded...\n");
                SM[i].binded=1;
            }
            else{
                pthread_mutex_unlock(&SM[i].mutex);
                perror("bind error");
                exit(1);
            }
        }
        pthread_mutex_unlock(&SM[i].mutex);
    }
}

void garbage_collector() {
    for(int i=0;i<MAX_SOCKETS;i++){
        pthread_mutex_lock(&SM[i].mutex);
        if(SM[i].pid!=0 &&(kill(SM[i].pid,0)==-1) &&(errno==ESRCH)){
            SM[i].pid=0;
            pthread_mutex_unlock(&SM[i].mutex);
            k_close(i);
            pthread_mutex_lock(&SM[i].mutex);
            close(SM[i].sockfd);
            if((SM[i].sockfd = socket(AF_INET,SOCK_DGRAM,0))<0){ //creating the sockets
                pthread_mutex_unlock(&SM[i].mutex);
                perror("socket creation failed");
                exit(1);
            }
            pthread_mutex_unlock(&SM[i].mutex);
            continue;
        }
        pthread_mutex_unlock(&SM[i].mutex);
    }
}

int main(){
    init_shared_memory();
    srand(time(NULL));
    pthread_t send_thread_id, receive_thread_id;
    if(pthread_create(&send_thread_id, NULL, sender_function, NULL) != 0){
        perror("Failed to create sender thread");
        return 1;
    }
    if(pthread_create(&receive_thread_id, NULL, receiver_function, NULL) != 0){
        perror("Failed to create receiver thread");
        return 1;
    }
    pthread_join(send_thread_id, NULL);
    pthread_join(receive_thread_id, NULL);

    return 0;
}





