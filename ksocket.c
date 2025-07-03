/*
===================================== 
Assignment 4 Submission 
Name: Satkuri Sumith Chandra 
Roll number: 22CS10067 
===================================== 
*/
#include "ksocket.h"

ksocket* SM =NULL;

void init_shared_memory() {
    int shmid = shmget(SHM_KEY,0,0);
    printf("shmid = %d\n",shmid);
    fflush(stdout);
    if (shmid < 0) {
        printf("no shmkey\n");
        shmid = shmget(SHM_KEY,MAX_SOCKETS*sizeof(ksocket),IPC_CREAT|0666);
        printf("shmid = %d",shmid);
        fflush(stdout);
        if (shmid < 0) {
            perror("shmget failed");
            exit(1);
        }
        SM = (ksocket *)shmat(shmid, NULL, 0);
        if (SM == (void *)-1) {
            perror("shmat failed");
            exit(1);
        }
        memset(SM, 0, MAX_SOCKETS * sizeof(ksocket));
        for (int i = 0; i < MAX_SOCKETS; i++){
            pthread_mutex_init(&SM[i].mutex, NULL);
            if((SM[i].sockfd = socket(AF_INET,SOCK_DGRAM,0))<0){ //creating the sockets
                perror("socket creation failed");
                exit(1);
            }
            SM[i].is_free=1;
        }
    }
    else{
        SM = (ksocket *)shmat(shmid, NULL, 0);
        if (SM == (void *)-1) {
            perror("shmat failed");
            exit(1);
        }
    }
}

int k_socket(int domain,int type,int protocol){
    if(SM==NULL) init_shared_memory();
    
    for(int i=0;i<MAX_SOCKETS;i++){
        printf("entered sock %d\n",i);
        pthread_mutex_lock(&SM[i].mutex);
        if(SM[i].is_free){
            SM[i].is_free=0;
            SM[i].pid = getpid();
            SM[i].timestamp =0;
            SM[i].send_seq_no=1;
            SM[i].send_buffer_size=0;
            SM[i].send_window.base=1;
            SM[i].send_window.wind_size=0;
            SM[i].recieve_window.base=1;
            SM[i].recieve_window.exp_seq=1;
            SM[i].recieve_buffer_size=0;
            SM[i].reciever_rwnd=BUFFER_SIZE;
            SM[i].binded=-1;
            SM[i].total_messages=0;
            SM[i].total_transmissions=0;
            pthread_mutex_unlock(&SM[i].mutex);
            return i;
        }
        pthread_mutex_unlock(&SM[i].mutex);
    }
    return -1;
}

int k_bind(int sockfd_ind,char src_ip[],int src_port,char dest_ip[],int dest_port,struct sockaddr_in* dest_addr){
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(src_ip);
    addr.sin_port = htons(src_port);
    pthread_mutex_lock(&SM[sockfd_ind].mutex);
    if (SM[sockfd_ind].pid != getpid()){
        errno = ERRORNP;
        printf("no pid in kbind...\n");
        pthread_mutex_unlock(&SM[sockfd_ind].mutex);
        return -1;
    }
    if (SM[sockfd_ind].is_free){
        printf("not is free in kbind...\n");
        pthread_mutex_unlock(&SM[sockfd_ind].mutex);
        return -1;
    }
    //Updates SM with dest ip and port
    SM[sockfd_ind].dest_IP =dest_ip;
    SM[sockfd_ind].dest_port = dest_port;
    memcpy(&SM[sockfd_ind].my_addr,&addr,sizeof(addr));
    memcpy(&SM[sockfd_ind].dest_addr,dest_addr,sizeof(struct sockaddr));
    SM[sockfd_ind].binded=0;

    while(SM[sockfd_ind].binded!=1){
        pthread_mutex_unlock(&SM[sockfd_ind].mutex);
        usleep(100000);
        pthread_mutex_lock(&SM[sockfd_ind].mutex);
    }
    pthread_mutex_unlock(&SM[sockfd_ind].mutex);
    printf("binding done\n");
    return 1;
}

int k_sendto(int sockfd_ind,char msg[],int msg_size,struct sockaddr* dest_addr,socklen_t dest_addr_len){
    if(sockfd_ind<0 || sockfd_ind>=MAX_SOCKETS){
        perror("Invalid socket descriptor\n");
        return -1;
    }
    pthread_mutex_lock(&SM[sockfd_ind].mutex);
    if(SM[sockfd_ind].is_free==1){
        errno = ENOTBOUND;
        pthread_mutex_unlock(&SM[sockfd_ind].mutex);
        return -1;
    }
    int sockfd = SM[sockfd_ind].sockfd;
    if(SM[sockfd_ind].send_buffer_size==BUFFER_SIZE){
        errno = ENOSPACE;
        pthread_mutex_unlock(&SM[sockfd_ind].mutex);
        return -1;
    }
    memcpy(SM[sockfd_ind].send_buffer[(SM[sockfd_ind].send_seq_no) % BUFFER_SIZE], msg, strlen(msg) + 1);
    SM[sockfd_ind].send_buffer_size++;
    SM[sockfd_ind].total_messages++;
    SM[sockfd_ind].send_seq_no = (SM[sockfd_ind].send_seq_no+1);
    printf("Added to send buffer\n");
    pthread_mutex_unlock(&SM[sockfd_ind].mutex);
    return 0;
}

int k_recvfrom(int sockfd_ind,char msg[],int msg_size,struct sockaddr* src_addr,socklen_t src_addr_len){
    if(sockfd_ind<0||sockfd_ind>=MAX_SOCKETS){
        perror("Invalid socket descriptor\n");
        return -1;
    }
    pthread_mutex_lock(&SM[sockfd_ind].mutex);
    if(SM[sockfd_ind].is_free==1){
        errno = ENOTBOUND;
        pthread_mutex_unlock(&SM[sockfd_ind].mutex);
        return -1;
    }
    int base = (SM[sockfd_ind].recieve_window.base)%BUFFER_SIZE;
    int sockfd = SM[sockfd_ind].sockfd;
    if(SM[sockfd_ind].recieve_buffer_size==0||SM[sockfd_ind].recieve_window.recieved[base]==0){
        errno = ENOMESSAGE;
        pthread_mutex_unlock(&SM[sockfd_ind].mutex);
        return -1;
    }
    if(SM[sockfd_ind].recieve_buffer_size>0&&SM[sockfd_ind].recieve_window.recieved[base]==1){
        strcpy(msg,SM[sockfd_ind].recieve_buffer[base]);
        memset(SM[sockfd_ind].recieve_buffer[base],0,MSG_SIZE);
        SM[sockfd_ind].recieve_window.recieved[base]=0;
        SM[sockfd_ind].recieve_window.base = (SM[sockfd_ind].recieve_window.base+1);
        SM[sockfd_ind].recieve_buffer_size--;
        printf("Recieved from recieve buffer\n");
    }
    pthread_mutex_unlock(&SM[sockfd_ind].mutex);
    return 0;
}

int k_close(int sockfd_ind){
    if(sockfd_ind<0||sockfd_ind>=MAX_SOCKETS){
        perror("Invalid socket descriptor\n");
        return -1;
    }
    pthread_mutex_lock(&SM[sockfd_ind].mutex);
    if(SM[sockfd_ind].is_free){
        pthread_mutex_unlock(&SM[sockfd_ind].mutex);
        return -1;
    }
    while(SM[sockfd_ind].send_buffer_size!=0){
        pthread_mutex_unlock(&SM[sockfd_ind].mutex);
        sleep(1);
        pthread_mutex_lock(&SM[sockfd_ind].mutex);
    }
    double avg_trans = ((double)SM[sockfd_ind].total_transmissions/SM[sockfd_ind].total_messages);
    printf("Total Transmissions = %d\n",SM[sockfd_ind].total_transmissions);
    printf("Average Transmissions = %lf\n",avg_trans);
    SM[sockfd_ind].is_free = 1;
    SM[sockfd_ind].binded=-1;
    SM[sockfd_ind].recieve_buffer_size=0;
    SM[sockfd_ind].send_buffer_size=0;
    memset(&SM[sockfd_ind].recieve_window,0,sizeof(rwnd));
    memset(&SM[sockfd_ind].send_window,0,sizeof(swnd));
    memset(&SM[sockfd_ind].send_buffer,0,sizeof(sizeof(SM[sockfd_ind].send_buffer)));
    memset(&SM[sockfd_ind].recieve_buffer,0,sizeof(sizeof(SM[sockfd_ind].recieve_buffer)));
    memset(&SM[sockfd_ind].my_addr,0,sizeof(struct sockaddr_in));
    memset(&SM[sockfd_ind].dest_addr,0,sizeof(struct sockaddr_in));
    pthread_mutex_unlock(&SM[sockfd_ind].mutex);
    return 1;
}

int dropmessage(float p1){
    float ran = rand()%100;
    p1*=100;
    if(ran<p1){
        return 1;
    }
    return 0;
}