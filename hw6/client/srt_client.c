#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include "string.h"
#include "srt_client.h"
#define VNAME(x) #x
#define MAX_THREAD_NUM 11

typedef struct client_tcb client_tcb_t;
typedef struct segBuf segBuf_t;
typedef struct port_sockfd_pair{
  int port;
  int sock;
}port_sock;

int overlay_conn;
const int TCB_TABLE_SIZE = MAX_TRANSPORT_CONNECTIONS;
const int CHK_STAT_INTERVAL_NS = 50;
pthread_t threads[MAX_THREAD_NUM];
client_tcb_t **tcb_table;
port_sock **p2s_hash_t;
int thread_count = 0;

/*interfaces to application layer*/

//
//
//  SRT socket API for the client side application.
//  ===================================
//
//  In what follows, we provide the prototype definition for each call and limited pseudo code representation
//  of the function. This is not meant to be comprehensive - more a guideline.
//
//  You are free to design the code as you wish.
//
//  NOTE: When designing all functions you should consider all possible states of the FSM using
//  a switch statement (see the Lab4 assignment for an example). Typically, the FSM has to be
// in a certain state determined by the design of the FSM to carry out a certain action.
//
//  GOAL: Your goal for this assignment is to design and implement the 
//  protoypes below - fill the code.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


// srt client initialization
//
// This function initializes the TCB table marking all entries NULL. It also initializes
// a global variable for the overlay TCP socket descriptor ‘‘conn’’ used as input parameter
// for snp_sendseg and snp_recvseg. Finally, the function starts the seghandler thread to
// handle the incoming segments. There is only one seghandler for the client side which
// handles call connections for the client.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

void srt_client_init(int conn) {
  printf("<func: %s>\n", __func__);
  overlay_conn = conn;

  // init tcb table and port/sock mapping
  int i;
  tcb_table = (client_tcb_t**) malloc(TCB_TABLE_SIZE * sizeof(client_tcb_t*));
  for (i = 0; i < TCB_TABLE_SIZE; i++){
    *(tcb_table + i) = NULL;
  }
  p2s_hash_t = (port_sock**) malloc(TCB_TABLE_SIZE * sizeof(port_sock*));
  for (i = 0; i < TCB_TABLE_SIZE; i++){
    *(p2s_hash_t + i) = NULL;
  }

  bzero(&threads, sizeof(pthread_t) * MAX_THREAD_NUM);
  // handling new coming request, creating new thread
  int pthread_err = pthread_create(threads + (thread_count++), NULL,
    (void *) seghandler, (void *) NULL);
  if (pthread_err != 0) {
    printf("%s: Create thread Failed!\n", __func__);
    return;
  }

  return;
}

// Create a client tcb, return the sock
//
// This function looks up the client TCB table to find the first NULL entry, and creates
// a new TCB entry using malloc() for that entry. All fields in the TCB are initialized
// e.g., TCB state is set to CLOSED and the client port set to the function call parameter
// client port.  The TCB table entry index should be returned as the new socket ID to the client
// and be used to identify the connection on the client side. If no entry in the TC table
// is available the function returns -1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int srt_client_sock(unsigned int client_port) {
  printf("<func: %s>\n", __func__);
  int idx;
  // find the first NULL, and create a tcb entry
  for (idx = 0; idx < TCB_TABLE_SIZE; idx++){
    if (tcb_table[idx] == NULL) {
      // creat a tcb entry
      tcb_table[idx] = (client_tcb_t*) malloc(sizeof(client_tcb_t));
      if(init_tcb(idx, client_port) == -1) 
        printf("%s: hash table insert failed!\n", __func__);
      printf("sock on port %d created\n", client_port);
      return idx;
    }
  }
  return -1;
}

int init_tcb(int sockfd, int port) {
  client_tcb_t* tcb = tcb_table[sockfd];
  tcb->svr_nodeID = 0;  // currently unused
  tcb->svr_portNum = 0; // @TODO: where to get it??, will be initialized latter  
  tcb->client_nodeID = topology_getMyNodeID();
  tcb->client_portNum = port;
  tcb->state = CLOSED; 
  tcb->next_seqNum = 0; 
  tcb->unAck_segNum = 0; 

    // init buffer
  tcb->bufMutex = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t)); 
  pthread_mutex_init(tcb->bufMutex, NULL);
  tcb->sendBufHead = NULL; 
  tcb->sendBufunSent = NULL; 
  tcb->sendBufTail = NULL; 

  // init hash table
  int i;
  for(i = 0; i < TCB_TABLE_SIZE; i++) {
    int hash_idx = (port + i) % TCB_TABLE_SIZE; // hash function
    if(p2s_hash_t[hash_idx] == NULL) {
      port_sock* p2s = (port_sock*) malloc(sizeof(port_sock));
      p2s->port = port;
      p2s->sock = sockfd;
      p2s_hash_t[hash_idx] = p2s;
      printf("hash table entry %d -> %d added\n", port, sockfd);
      return 0;
    }
  }
  return -1;
}

// Connect to a srt server
//
// This function is used to connect to the server. It takes the socket ID and the
// server’s port number as input parameters. The socket ID is used to find the TCB entry.
// This function sets up the TCB’s server port number and a SYN segment to send to
// the server using snp_sendseg(). After the SYN segment is sent, a timer is started.
// If no SYNACK is received after SYNSEG_TIMEOUT timeout, then the SYN is
// retransmitted. If SYNACK is received, return 1. Otherwise, if the number of SYNs
// sent > SYN_MAX_RETRY,  transition to CLOSED state and return -1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//


int srt_client_connect(int sockfd, int nodeID, unsigned int server_port) {
  printf("<func: %s>\n", __func__);
  // set the state of corresponding tcb entry
  if (state_transfer(sockfd, SYNSENT) == -1)
    printf("%s: state tranfer err!\n", __func__);
  if(tcb_table[sockfd] == NULL)
    printf("%s: tcb not found!\n", __func__);

  tcb_table[sockfd]->svr_portNum = server_port;
  tcb_table[sockfd]->svr_nodeID = nodeID;
  return keepTry(sockfd, SYNSENT, SYN_MAX_RETRY, SYN_TIMEOUT);
}

int is_timeout(struct timespec tstart, struct timespec tend, long timeout_ns) {
  // printf("<func: %s>\n", __func__);
  if(tend.tv_sec - tstart.tv_sec > 0)
    return 1;
  else if(tend.tv_nsec - tstart.tv_nsec > timeout_ns)
    return 1;
  else
    return 0;
}

void send_control_msg(int sockfd, int type) {
  printf("<func: %s>\n", __func__);
  if(tcb_table[sockfd] == NULL)
    printf("%s: tcb not found!\n", __func__);

  seg_t* segPtr = (seg_t*) malloc(sizeof(seg_t));
  bzero(segPtr, sizeof(seg_t));
  segPtr->header.src_port = tcb_table[sockfd]->client_portNum;
  segPtr->header.dest_port = tcb_table[sockfd]->svr_portNum;
  segPtr->header.checksum = 0;
  segPtr->header.type = type;

  // printf("%s: about to snp_sendseg \n", __func__);
  if(snp_sendseg(overlay_conn, tcb_table[sockfd]->svr_nodeID, segPtr) < 0)
    printf("%s: snp_sendseg fail \n", __func__);
  // else
    // printf("%s: snp_sendseg done \n", __func__);
}

// Send data to a srt server
//
// Send data to a srt server. You do not need to implement this for Lab4.
// We will use this in Lab5 when we implement a Go-Back-N sliding window.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int srt_client_send(int sockfd, void* data, unsigned int length) {
  printf("<func: %s>\n", __func__);
  if(tcb_table[sockfd] == NULL)
    printf("%s: tcb not found!\n", __func__);
  // else
    // printf("in func %s\n", __func__);

  int data_idx = 0, is_first = 1; 
   // mutual exclusion free for buffer operation
  // printf("%s: lock\n", __func__);
  pthread_mutex_lock(tcb_table[sockfd]->bufMutex);
  do {
    // printf("%s: while loop, data_idx %d, seg_idx %d \n", __func__, data_idx, seg_idx);
    // create buffer node
    segBuf_t* bufNode = (segBuf_t*) malloc(sizeof(segBuf_t));
    // create a new seg header
    bufNode->seg.header.src_port = tcb_table[sockfd]->client_portNum;
    bufNode->seg.header.dest_port = tcb_table[sockfd]->svr_portNum;
    bufNode->seg.header.seq_num = tcb_table[sockfd]->next_seqNum;
    bufNode->seg.header.type = DATA;
    bufNode->seg.header.length = 0;
    bufNode->seg.header.checksum = 0;
    bufNode->sentTime = 0;  // unsent bufeNode, default to 0
    
    // write the data into seg data area
    int len = MIN(length - data_idx, MAX_SEG_LEN);
    memcpy(bufNode->seg.data, data + data_idx, len);
    bufNode->seg.header.length += len;
    data_idx += len;

    // int c;
    // for(c = 0; c < len; c++) {
    //   printf("%d %c \n", *((char*)data + c), (char)(*((char*)data + c)));
    // }
    // for(c = 0; c < len; c++) {
    //   printf("%d %c \n", *(bufNode->seg.data + c), (char)(*(bufNode->seg.data + c)));
    // }
    char tmp[2048];
    memcpy(tmp, bufNode->seg.data, len);
    tmp[len] = '\0';
    printf("<func: %s>: seg data is %s\n", __func__, tmp);
    printf("<func: %s>: sockfd %d's length %d\n", __func__, sockfd, bufNode->seg.header.length);
    tcb_table[sockfd]->next_seqNum += bufNode->seg.header.length;
    sendBuf_append(tcb_table[sockfd], bufNode, is_first);
    if(is_first == 1) is_first = 0;

    // char testOutPut[MAX_SEG_LEN + 1];
    // strcat(testOutPut, "\0");
    // puts(testOutPut);

  } while(data_idx < length);

  sendBuf_instantSend(tcb_table[sockfd]);
  pthread_mutex_unlock(tcb_table[sockfd]->bufMutex);

  // create sendBuf_timer for this tcb buffer
  int pthread_err = pthread_create(threads + (thread_count++), NULL,
    (void *) sendBuf_timer, (void *) &tcb_table[sockfd]->client_portNum);
  if (pthread_err != 0) {
    printf("%s: Create thread Failed!\n", __func__);
    return;
  }
  // printf("%s: unlock\n", __func__);
  return 1;
}

void sendBuf_instantSend(client_tcb_t *tcb) {
  printf("<func: %s>\n", __func__);
  // check exception
  if(tcb == NULL)
    printf("err in %s: tcb entry is NUll!\n", __func__);
  if(tcb->sendBufunSent == NULL)
    printf("err in %s: sendBufunSent is NULL!\n", __func__);

  // send initial segments as much as possible
  while (tcb->unAck_segNum <= GBN_WINDOW && tcb->sendBufunSent != NULL) {
    // send seg
    if(snp_sendseg(overlay_conn, tcb->svr_nodeID, &tcb->sendBufunSent->seg) < 0)
      printf("err in %s: snp_sendseg fail \n", __func__);
    else {
      printf("<func: %s> send data with seqNum: %d\n", __func__, tcb->sendBufunSent->seg.header.seq_num);
      tcb->sendBufunSent->sentTime = (unsigned int)time(NULL);
    }
    tcb->unAck_segNum++;    
    tcb->sendBufunSent = tcb->sendBufunSent->next;
  }

}

// Disconnect from a srt server
//
// This function is used to disconnect from the server. It takes the socket ID as
// an input parameter. The socket ID is used to find the TCB entry in the TCB table.
// This function sends a FIN segment to the server. After the FIN segment is sent
// the state should transition to FINWAIT and a timer started. If the
// state == CLOSED after the timeout the FINACK was successfully received. Else,
// if after a number of retries FIN_MAX_RETRY the state is still FINWAIT then
// the state transitions to CLOSED and -1 is returned.

int srt_client_disconnect(int sockfd) {
  printf("<func: %s>\n", __func__);
  // set the state of corresponding tcb entry
  if (state_transfer(sockfd, FINWAIT) == -1)
    printf("%s: state tranfer err!\n", __func__);

  // send_control_msg(sockfd, FIN);
  // printf("FIN sent to sockfd %d\n", sockfd);
  if( keepTry(sockfd, FINWAIT, FIN_MAX_RETRY, FIN_TIMEOUT) == -1)
    printf("err in %s: cannot disconnect\n", __func__);
  // release the whole buf
  sendBuf_handleACK(sockfd, MAX_SEQ_NO);
}

int state_transfer(int sockfd, int new_state) {
  printf("<func: %s>\n", __func__);
  if(tcb_table[sockfd] == NULL)
    printf("%s: tcb not found!\n", __func__);

  if(new_state == CLOSED) {
    if (tcb_table[sockfd]->state == SYNSENT
      || tcb_table[sockfd]->state == FINWAIT) {
      tcb_table[sockfd]->state = CLOSED;
      printf("State of sockfd %d transfer to %s\n", sockfd, "CLOSED");
      return 0;
    }
  } else if (new_state == CONNECTED) {
    if (tcb_table[sockfd]->state == SYNSENT) {
      tcb_table[sockfd]->state = CONNECTED;
      printf("State of sockfd %d transfer to %s\n", sockfd, "CONNECTED");
      return 0;
    }
  } else if (new_state == SYNSENT) {
    if (tcb_table[sockfd]->state == CLOSED
      || tcb_table[sockfd]->state == SYNSENT) {
      tcb_table[sockfd]->state = SYNSENT;
      printf("State of sockfd %d transfer to %s\n", sockfd, "SYNSENT");
      return 0;
    }
  } else if (new_state == FINWAIT) {
    if (tcb_table[sockfd]->state == CONNECTED
      || tcb_table[sockfd]->state == FINWAIT) {
      tcb_table[sockfd]->state = FINWAIT;
      printf("State of sockfd %d transfer to %s\n", sockfd, "FINWAIT");
      return 0;
    }
  }
  return -1;
}

int keepTry(int sockfd, int action, int maxtry, long timeout) {
  printf("<func: %s>\n", __func__);
  if(tcb_table[sockfd] == NULL)
    printf("%s: tcb not found!\n", __func__);

  int try_cnt = 1;
  while(maxtry == -1 || try_cnt++ <= FIN_MAX_RETRY) {
    int type = action == SYNSENT ? SYN : FIN;
    send_control_msg(sockfd, type);
    struct timespec tstart={0,0}, tend={0,0};
    clock_gettime(CLOCK_MONOTONIC, &tstart);
    clock_gettime(CLOCK_MONOTONIC, &tend);
    while(timeout == -1 || !is_timeout(tstart, tend, timeout)) {
      if(action == SYNSENT 
        && tcb_table[sockfd]->state == CONNECTED)
        return 1;
      else if(action == FINWAIT
        && tcb_table[sockfd]->state == CLOSED)
        return 1;
      clock_gettime(CLOCK_MONOTONIC, &tend);
    }
  }
  if (state_transfer(sockfd, CLOSED) == -1)
    printf("%s: state tranfer err!\n", __func__);
  return -1;
}

// Close srt client

// This function calls free() to free the TCB entry. It marks that entry in TCB as NULL
// and returns 1 if succeeded (i.e., was in the right state to complete a close) and -1
// if fails (i.e., in the wrong state).
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int srt_client_close(int sockfd) {
  printf("<func: %s>\n", __func__);
  if(tcb_table[sockfd] == NULL)
    printf("%s: tcb not found!\n", __func__);
  if(tcb_table[sockfd]->state != CLOSED)
    return -1;

  // delete entry in hash table
  int port = tcb_table[sockfd]->client_portNum;
  int idx = p2s_hash_get_idx(port);
  if(sockfd == -1) printf("%s: Hash get sockfd idx err\n", __func__);
  free(p2s_hash_t[idx]);
  p2s_hash_t[idx] = NULL;
  printf("hash table entry %d -> %d deleted\n", port, sockfd);

  // free mutex
  free(tcb_table[sockfd]->bufMutex);
  tcb_table[sockfd]->bufMutex = NULL;

  // delete entry in tcb table
  free(tcb_table[sockfd]);
  tcb_table[sockfd] = NULL;

  return 1;
}

// The thread handles incoming segments
//
// This is a thread  started by srt_client_init(). It handles all the incoming
// segments from the server. The design of seghanlder is an infinite loop that calls snp_recvseg(). If
// snp_recvseg() fails then the overlay connection is closed and the thread is terminated. Depending
// on the state of the connection when a segment is received  (based on the incoming segment) various
// actions are taken. See the client FSM for more details.
//

void *seghandler(void *arg) {
  //printf("<func: %s>\n", __func__);
  int src_nodeID;
  seg_t* segPtr = (seg_t*) malloc(sizeof(seg_t));
  while(1) {
    if(snp_recvseg(overlay_conn, &src_nodeID, segPtr) == 1){
      int sockfd = p2s_hash_get_sock(segPtr->header.dest_port);
      if(sockfd == -1) printf("%s: Hash get sockfd err\n", __func__);
      if(segPtr->header.type == SYNACK){
        // printf("SYNACK get\n");
        if (state_transfer(sockfd, CONNECTED) == -1)
          printf("%s: state tranfer to CONNECTED err!\n", __func__);
      }
      else if(segPtr->header.type == FINACK){
        // printf("FINACK get\n");
        if (state_transfer(sockfd, CLOSED) == -1)
          printf("%s: state tranfer to CLOSED err!\n", __func__);
      }
      else if(segPtr->header.type == DATAACK){
        sendBuf_handleACK(sockfd, segPtr->header.ack_num);
      }
      else{
        printf("%s: unkown msg type!\n", __func__);
        return ;
      }
    }else{
      return ;
    }
  }
}

void sendBuf_handleACK(int sockfd, int ack_num) {
  // printf("<func: %s, ack_num %d>\n", __func__, ack_num);
  client_tcb_t *tcb = tcb_table[sockfd];
  // printf("%s: entry get\n", __func__);
  // printf("%s: lock\n", __func__);
  pthread_mutex_lock(tcb->bufMutex);
  // printf("%s: lock get\n", __func__);
  // if(tcb->sendBufTail->seg.header.seq_num < ack_num)
  //   printf("%s: Going to release all buf.\n", __func__);
  if(tcb == NULL)
    printf("%s: tcb is null!!\n", __func__);
  if(tcb->sendBufHead == NULL){
    printf("%s: sendBufHead is null, no need to clear buf\n", __func__);
  } else {
    while(tcb->sendBufHead != NULL 
        && tcb->sendBufHead->seg.header.seq_num <= ack_num) {
      // printf("%s: inside while!!\n", __func__);
      segBuf_t* tmpPtr = tcb->sendBufHead;
      tcb->sendBufHead = tcb->sendBufHead->next;
      // printf("%s: head to next\n", __func__);
      if(tmpPtr == tcb->sendBufunSent && ack_num != MAX_SEQ_NO)
        printf("err in %s: try to relase an unsent node\n", __func__);
      // printf("%s: going to free!!!\n", __func__);
      free(tmpPtr);
      tcb->unAck_segNum--;
    }
    // if the buffer is empty
    if( tcb->sendBufHead == NULL) {
      tcb->sendBufTail = tcb->sendBufTail = tcb->sendBufunSent = NULL;
    }
  }
  pthread_mutex_unlock(tcb->bufMutex); 
}

void sendBuf_append(client_tcb_t *tcb, segBuf_t* bufNode, int is_first) {
  printf("<func: %s>\n", __func__);
  if(tcb == NULL)
    printf("err in %s: tcb entry is NUll!\n", __func__);
  if(bufNode == NULL)
    printf("err in %s: bufNode is NUll!\n", __func__);

  bufNode->next = NULL;
  if(tcb->sendBufTail == NULL){
    // the buffer is currently empty
    tcb->sendBufTail = tcb->sendBufHead = bufNode;
  } else {
    tcb->sendBufTail->next = bufNode;
    tcb->sendBufTail = tcb->sendBufTail->next;
  }

  // init sentBufUnsent, if needed
  if(tcb->sendBufunSent == NULL && is_first == 1) {
    tcb->sendBufunSent = bufNode;
  }
}

int p2s_hash_get_sock(int port) {
  int hash_idx = p2s_hash_get_idx(port);
  return hash_idx == -1 ? hash_idx : p2s_hash_t[hash_idx]->sock;
}

int p2s_hash_get_idx(int port) {
  // printf("<func: %s>\n", __func__);
  int i;
  // printf("%s: search for port %d\n", __func__, port);
  for(i = 0; i < TCB_TABLE_SIZE; i++) {
    int hash_idx = (port + i) % TCB_TABLE_SIZE; // hash function
    // if(p2s_hash_t[hash_idx] != NULL )
      // printf("%d %d\n", p2s_hash_t[hash_idx]->sock, p2s_hash_t[hash_idx]->port);
    // printf("<func: %s>: checking idx: %d \n", __func__, hash_idx);
    if(p2s_hash_t[hash_idx] != NULL 
      && p2s_hash_t[hash_idx]->port == port) {
      // printf("<func: %s> return idx\n", __func__);
      return hash_idx;
    }
  }
  // printf("<func: %s> return -1\n", __func__);
  return -1;  
}

void *sendBuf_timer(void* c_port) { 
  int client_port = *((int*)c_port);
  printf("<func: %s>\n", __func__);
  // printf("in func %s\n", __func__);
  int sockfd = p2s_hash_get_sock(client_port);
  if(sockfd == -1)
    printf("<func: %s>: sockfd error on port %d\n", __func__, client_port);

  client_tcb_t *tcb = tcb_table[sockfd];
  while(tcb->unAck_segNum != 0 || tcb->sendBufHead != NULL) {
    sleep(SENDBUF_POLLING_INTERVAL);
    // mutual exclusion free for buffer operation
    // printf("%s: lock\n", __func__);
    pthread_mutex_lock(tcb->bufMutex);
    int bgn_idx = 0; 
    segBuf_t* bufPtr = tcb->sendBufHead;
    // try to sent bufNode within GBN_WINDOW
    while (bgn_idx < GBN_WINDOW && bufPtr != NULL) {
      time_t t0 = bufPtr->sentTime;
      if (t0 == 0 
        || difftime(time(NULL), t0) * 1000 >= DATA_TIMEOUT) {
        // if it is not sent yet
        // or if this bufNode sent before and reach timeout, then resend
        if(snp_sendseg(overlay_conn, tcb->svr_nodeID, &bufPtr->seg) < 0)
          printf("err in %s: snp_sendseg fail \n", __func__);
        else {
          printf("<func: %s> send data with seqNum: %d\n", __func__, tcb->sendBufunSent->seg.header.seq_num);
          bufPtr->sentTime = (unsigned int)time(NULL);
        }
      }
      tcb->sendBufunSent = bufPtr->next;
      bufPtr = bufPtr->next;
      bgn_idx++;
    }
    pthread_mutex_unlock(tcb->bufMutex);
    // printf("%s: unlock\n", __func__);
  }
}
