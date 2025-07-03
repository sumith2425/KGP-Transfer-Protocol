# Assignment 4 - Reliable UDP using Shared Memory

**Name:** Satkuri Sumith Chandra  
**Roll Number:** 22CS10067

---

## Overview

This assignment implements a reliable UDP-like communication protocol using shared memory and sliding window mechanism.

---

## Data Structures

### `ksocket`

```c
typedef struct {
    int is_free;              // 1 -> free, 0 -> not free
    int binded;               // -1 -> not binded, 0 -> binding in progress, 1 -> binded
    int pid;                  // process ID
    int sockfd;               // UDP socket file descriptor
    char* dest_IP;            // destination IP address
    int dest_port;            // destination port
    struct sockaddr_in dest_addr, my_addr; // socket addresses
    char send_buffer[BUFFER_SIZE][MSG_SIZE];
    char recieve_buffer[BUFFER_SIZE][MSG_SIZE];
    char recent_ack[600];     // for zero rwnd scenario
    time_t timestamp;         // tracks unacknowledged time
    time_t rwnd_time;         // tracks time when receiver window = 0

    int recieve_buffer_size;  // in-order messages not yet read
    int send_buffer_size;     // messages in send buffer
    int send_seq_no;          // seq no for next message to send

    int reciever_rwnd;
    swnd send_window;
    rwnd recieve_window;
    pthread_mutex_t mutex;

    int total_transmissions;
    int total_messages;
} ksocket;
```

- Shared memory array of sockets: `ksocket* SM`

---

### `swnd`

```c
typedef struct {
    int seq_no[WINDOW_SIZE];
    int base;
    int wind_size;
} swnd;
```

---

### `rwnd`

```c
typedef struct {
    int recieved[WINDOW_SIZE];
    int base;
    int exp_seq;
} rwnd;
```

---

## File Descriptions and Functions

### `initksocket.c`

- **`receiver_function`**:  
  Checks for binding and receives packets via `select()`. Updates buffers on ACK/data.

- **`sender_function`**:  
  Periodically (T/2 seconds) runs garbage collector and manages retransmissions. Handles `rwnd = 0` case by resending last ACK.

- **`bind_function`**:  
  Performs actual socket binding if `binded == 0`.

- **`garbage_collector`**:  
  Frees up dead socket entries and reinitializes them.

---

### `ksocket.c`

- **`init_shared_memory`**:  
  Initializes shared memory and socket pool.

- **`k_socket(domain, type, protocol)`**:  
  Allocates a free socket and initializes fields.

- **`k_bind(sockfd_ind, src_ip, src_port, dest_ip, dest_port, dest_addr)`**:  
  Initiates bind by setting `binded = 0` and waits for bind completion.

- **`k_sendto(sockfd_ind, msg, msg_size, dest_addr, addr_len)`**:  
  Adds message to send buffer.

- **`k_recvfrom(sockfd_ind, msg, msg_size, src_addr, addr_len)`**:  
  Retrieves message from receive buffer if available.

- **`k_close(sockfd_ind)`**:  
  Waits for sender buffer to empty before closing socket and marking as free.

- **`dropmessage(p1)`**:  
  Returns `1` with probability `p1` (used for simulating packet loss).

---

## Experimental Results

| Probability | Total Transmissions | Average Transmissions |
|-------------|---------------------|------------------------|
| 0.05        | 394                 | 1.201                  |
| 0.10        | 532                 | 1.622                  |
| 0.15        | 558                 | 1.701                  |
| 0.20        | 576                 | 1.756                  |
| 0.25        | 647                 | 1.973                  |
| 0.30        | 817                 | 2.491                  |
| 0.35        | 859                 | 2.619                  |
| 0.40        | 1058                | 3.226                  |
| 0.45        | 1154                | 3.518                  |
| 0.50        | 1431                | 4.363                  |

Original messages: 328

---

## How to Run

### Step-by-step

1. **Terminal 1:**
   ```bash
   make
   ./init
   ```

2. **Terminal 2:**
   ```bash
   ./user1 testfile.txt
   ```

3. **Terminal 3:**
   ```bash
   ./user2 recieved.txt
   ```

### Notes:

- `user1` exits automatically once sender buffer is empty.
- Use `Ctrl+C` to terminate `user2` and `init`.
- Before restarting `init`, use:
  ```bash
  ipcrm -m <shmid>
  ```
  Get `shmid` via:
  ```bash
  ipcs
  ```
