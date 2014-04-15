#include <stdlib.h>
#include <sys/socket.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include "srt_server.h"
#include "../common/constants.h"
#define MAX_THREAD_NUM 11

typedef struct client_tcb svr_tcb_t;
typedef struct port_sockfd_pair{
  int port;
  int sock;
}port_sock;

int overlay_conn;
const int TCB_TABLE_SIZE = 11;
const int CHK_STAT_INTERVAL_NS = 50;
pthread_t threads[MAX_THREAD_NUM];
svr_tcb_t **tcb_table;
port_sock **p2s_hash_t;

/*interfaces to application layer*/

//
//
//  SRT socket API for the server side application.
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
//  GOAL: Your job is to design and implement the prototypes below - fill in the code.
//

// srt server initialization
//
// This function initializes the TCB table marking all entries NULL. It also initializes
// a global variable for the overlay TCP socket descriptor ‘‘conn’’ used as input parameter
// for snp_sendseg and snp_recvseg. Finally, the function starts the seghandler thread to
// handle the incoming segments. There is only one seghandler for the server side which
// handles call connections for the client.
//

void srt_server_init(int conn) {
  int thread_count, i;
  overlay_conn = conn;

  // init tcb table and port/sock mapping
  tcb_table = (svr_tcb_t**) malloc(TCB_TABLE_SIZE * sizeof(svr_tcb_t*));
  for (i = 0; i < TCB_TABLE_SIZE; i++){
    *(tcb_table + i) = NULL;
  }
  p2s_hash_t = (port_sock**) malloc(TCB_TABLE_SIZE * sizeof(port_sock*));
  for (i = 0; i < TCB_TABLE_SIZE; i++){
    *(p2s_hash_t + i) = NULL;
  }

  // handling new coming request
  bzero(&threads, sizeof(pthread_t) * MAX_THREAD_NUM);
  // creating new thread
  int pthread_err = pthread_create(threads + (thread_count++), NULL,
    (void *) seghandler, (void *) NULL);
  if (pthread_err != 0) {
    printf("Create thread Failed!\n");
    return;
  }

  return;
}

// Create a server sock
//
// This function looks up the client TCB table to find the first NULL entry, and creates
// a new TCB entry using malloc() for that entry. All fields in the TCB are initialized
// e.g., TCB state is set to CLOSED and the server port set to the function call parameter
// server port.  The TCB table entry index should be returned as the new socket ID to the server
// and be used to identify the connection on the server side. If no entry in the TCB table
// is available the function returns -1.

int srt_server_sock(unsigned int port) {
  int idx;
  // find the first NULL, and create a tcb entry
  for (idx = 0; idx < TCB_TABLE_SIZE; idx++){
    if (tcb_table[idx] == NULL) {
      // creat a tcb entry
      tcb_table[idx] = (svr_tcb_t*) malloc(sizeof(svr_tcb_t));
      if(init_tcb(tcb_table[idx], port) == -1) 
        printf("hash table insert failed!\n");
      return idx;
    }
  }
  return -1;
}

int init_tcb(svr_tcb_t* tcb_t, int port) {
  tcb_t->svr_nodeID = 0;  // currently unused
  tcb_t->svr_portNum = port;   
  tcb_t->client_nodeID = 0;  // currently unused
  tcb_t->client_portNum = 0; // @TODO: where to get it??, will be initialized latter
  tcb_t->state = CLOSED; 
  tcb_t->next_seqNum = 0; 
  tcb_t->bufMutex = NULL; 
  tcb_t->sendBufHead = NULL; 
  tcb_t->sendBufunSent = NULL; 
  tcb_t->sendBufTail = NULL; 
  tcb_t->unAck_segNum = 0; 

  int i;
  for(i = 0; i < TCB_TABLE_SIZE; i++) {
    int hash_idx = (port + i) % TCB_TABLE_SIZE; // hash function
    if(p2s_hash_t[hash_idx] == NULL) {
      port_sock* p2s = (port_sock*) malloc(sizeof(port_sock));
      p2s_hash_t[hash_idx] = p2s;
      return 0;
    }
  }
  return -1;
}

// Accept connection from srt client
//
// This function gets the TCB pointer using the sockfd and changes the state of the connetion to
// LISTENING. It then starts a timer to ‘‘busy wait’’ until the TCB’s state changes to CONNECTED
// (seghandler does this when a SYN is received). It waits in an infinite loop for the state
// transition before proceeding and to return 1 when the state change happens, dropping out of
// the busy wait loop. You can implement this blocking wait in different ways, if you wish.
//

int srt_server_accept(int sockfd) {
  // set the state of corresponding tcb entry
  tcb_table[sockfd]->state = LISTENING;

  // timer
  return keep_try(sockfd, LISTENING, -1, -1);
}

int is_timeout(struct timespec tstart, struct timespec tend, long timeout_ns) {
  if(tend.tv_sec - tstart.tv_sec > 0)
    return 1;
  else if(tend.tv_nsec - tstart.tv_nsec > timeout_ns)
    return 1;
  else
    return 0;
}

void send_control_msg(int sockfd, int action) {
  seg_t* segPtr = (seg_t*) malloc(sizeof(seg_t));
  segPtr->header.src_port = tcb_table[sockfd]->client_portNum;
  segPtr->header.dest_port = tcb_table[sockfd]->svr_portNum;

  // configure control msg type
  if (action == LISTENING){
      segPtr->header.type = SYNACK;
  } else {
    printf("send_control_msg: action not found!s\n");
  }

  sendseg(overlay_conn, segPtr);
}

int keep_try(int sockfd, int action, int maxtry, long timeout) {
  int try_cnt = 1;
  while(maxtry == -1 || try_cnt++ <= FIN_MAX_RETRY) {
    struct timespec tstart={0,0}, tend={0,0};
    clock_gettime(CLOCK_MONOTONIC, &tstart);
    clock_gettime(CLOCK_MONOTONIC, &tend);
    while(timeout == -1 || !is_timeout(tstart, tend, timeout)) {
      sleep(50);
      if(action == LISTENING 
        && tcb_table[sockfd]->state == CONNECTED)
        return 1;
      else if(action == FINWAIT
        && tcb_table[sockfd]->state == CLOSED)
        return 1;
      else
        printf("keep_try: action not found!s\n");
      clock_gettime(CLOCK_MONOTONIC, &tend);
    }
  }
  tcb_table[sockfd]->state = CLOSED;
  return -1;
}

// Receive data from a srt client
//
// Receive data to a srt client. Recall this is a unidirectional transport
// where DATA flows from the client to the server. Signaling/control messages
// such as SYN, SYNACK, etc.flow in both directions. You do not need to implement
// this for Lab4. We will use this in Lab5 when we implement a Go-Back-N sliding window.
//
int srt_server_recv(int sockfd, void* buf, unsigned int length) {
	return 1;
}

// Close the srt server
//
// This function calls free() to free the TCB entry. It marks that entry in TCB as NULL
// and returns 1 if succeeded (i.e., was in the right state to complete a close) and -1
// if fails (i.e., in the wrong state).
//

int srt_server_close(int sockfd) {
  if(tcb_table[sockfd]->state != CLOSED)
    return -1;
  else{
    free(tcb_table[sockfd]);
    tcb_table[sockfd] = NULL;
    return 1;
  }
}

// Thread handles incoming segments
//
// This is a thread  started by srt_server_init(). It handles all the incoming
// segments from the client. The design of seghanlder is an infinite loop that calls snp_recvseg(). If
// snp_recvseg() fails then the overlay connection is closed and the thread is terminated. Depending
// on the state of the connection when a segment is received  (based on the incoming segment) various
// actions are taken. See the client FSM for more details.
//

void *seghandler(void* arg) {
  return 0;
}
