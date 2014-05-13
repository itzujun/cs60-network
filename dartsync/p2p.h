#ifndef DARTSYNCP2P
#define DARTSYNCP2P
#include "config.h"

void* p2p_listening(void* arg);

void* p2p_download(void* arg);

void* p2p_upload(void* arg);






/**********some helper structure for testing************/

//each file can be represented as a node in file table
typedef struct node{
//the size of the file
  int size;
//the name of the file
  char *name;
//the timestamp when the file is modified or created
  unsigned long int timestamp;
//pointer to build the linked list
  struct node *pNext;
//for the file table on peers, it is the ip address of the peer
//for the file table on tracker, it records the ip of all peers which has the
//newest edition of the file
  char *newpeerip;
}Node,*pNode;

#endif