#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include "srt_client.h"

#define MAX_THREAD_NUM 2

int svr_conn;
const int TCB_TABLE_SIZE = 11;
const int CHK_STAT_INTERVAL_NS = 50;
pthread_t threads[MAX_THREAD_NUM];
client_tcb **tcb_table;

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
  svr_conn = conn;

  tcb_table = (client_tcb**) malloc(TCB_TABLE_SIZE*sizeof(client_tcb*));
  for (int i = 0; i < TCB_TABLE_SIZE; i++){
    *(tcb_table + i) = NULL;
  }

  // handling new coming request
  bzero(&threads, sizeof(pthread_t) * MAX_THREAD_NUM);
  // creating new thread
  int pthread_err = pthread_create(threads + (thread_count++), NULL,
    seghandler, (void *) NULL);
  if (pthread_err != 0) {
    printf("Create thread Failed!\n");
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
  // find the first NULL, and create a tcb entry
  for (int idx = 0; idx < TCB_TABLE_SIZE; idx++){
    if (tcb_table[idx] == NULL) {
      // creat a tcb entry
      tcb_table[idx] = (client_tcb*) malloc(sizeof(client_tcb_t));
      init_tcb(tcb_table[idx]);
      return idx;
    }
  }
  return -1;
}

void init_tcb(client_tcb_t* tcb_t) {
  tcb_t->svr_nodeID = 0;  // currently unused
  tcb_t->svr_portNum = 0; // @TODO: where to get it??, will be initialized latter  
  tcb_t->client_nodeID = 0;  // currently unused
  tcb_t->client_portNum = client_port;
  tcb_t->state = CLOSED; 
  tcb_t->next_seqNum = 0; 
  tcb_t->bufMutex = NULL; 
  tcb_t->sendBufHead = NULL; 
  tcb_t->sendBufunSent = NULL; 
  tcb_t->sendBufTail = NULL; 
  tcb_t->unAck_segNum = 0; 
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


int srt_client_connect(int sockfd, unsigned int server_port) {
  // set the state of corresponding tcb entry
  tcb_table[sockfd]->svr_portNum = server_port;
  tcb_table[sockfd]->state = SYNSENT;
  send_control_msg(sockfd, server_port, SYNSENT);

  // timer
  int try_cnt = 1;
  while(try_cnt++ <= SYN_MAX_RETRY) {
    struct timespec tstart={0,0}, tend={0,0};
    clock_gettime(CLOCK_MONOTONIC, &tstart);
    clock_gettime(CLOCK_MONOTONIC, &tend);
    while(!is_syn_timeout(tstart, tend, SYNSENT)) {
      sleep(50);
      if(tcb_table[sockfd]->state == CONNECTED)
        return 1;
    }
  }
  tcb_table[sockfd]->state = CLOSED;

  return -1;
}

int is_timeout(struct timespec tstart, struct timespec tend, int action) {
  if(tend.tv_sec - tstart.tv_sec > 0)
    return 1;
  else if(tend.tv_nsec - tstart.tv_nsec > SYNSEG_TIMEOUT_NS)
    return 1;
  else
    return 0;
}

void send_control_msg(int sockfd, int action) {
  seg_t* segPtr = (seg_t*) malloc(sizeof(seg_t));
  segPtr->src_port = tcb_table[sockfd]->client_portNum;
  segPtr->dest_port = tcb_table[sockfd]->svr_portNum;

  // configure control msg type
  if (action == SYNSENT){
      segPtr->type = SYN;
  }

  sendseg(svr_conn, segPtr);
}

// Send data to a srt server
//
// Send data to a srt server. You do not need to implement this for Lab4.
// We will use this in Lab5 when we implement a Go-Back-N sliding window.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int srt_client_send(int sockfd, void* data, unsigned int length) {
 return 1;
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
  return 0;
}


// Close srt client

// This function calls free() to free the TCB entry. It marks that entry in TCB as NULL
// and returns 1 if succeeded (i.e., was in the right state to complete a close) and -1
// if fails (i.e., in the wrong state).
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int srt_client_close(int sockfd) {
	return 0;
}

// The thread handles incoming segments
//
// This is a thread  started by srt_client_init(). It handles all the incoming
// segments from the server. The design of seghanlder is an infinite loop that calls snp_recvseg(). If
// snp_recvseg() fails then the overlay connection is closed and the thread is terminated. Depending
// on the state of the connection when a segment is received  (based on the incoming segment) various
// actions are taken. See the client FSM for more details.
//

void *seghandler(void* arg) {
  return 0;
}



