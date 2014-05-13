#ifndef DARTSYNCP2P
#define DARTSYNCP2P
#include "config.h"

#define REQ 0
#define REQFIN 1
#define REREQ 2

typedef struct p2p_msg{
  int type;
  //the name of the file
  char *name;
  // file szie
  int size;
  // the pieceNum in the file, start from 0
  int pNum;
  // port
  int srcPort;
  int destPort;
  // ip address
  char* srcIp;
  char* destIp;
}P2PMsg;

/**
pieces maintain table, 
*/
typedef struct p2p_pieceTable{
  // filename
  char* name;
  // size
  int size;
  // count current pieces
  int piecesDone;
  // store total pieces
  int totalPieces;
  // keep track of which one is done
  int* pieces;
  // keep track of which one is done
  int* peers;
  // the data area
  char* data;
  // pointer
  pEntry* next;
}pEntry;

void* p2p_listening(void* arg);

void* p2p_download(void* arg);

void* p2p_upload(void* arg);

int p2p_assemble(pEntry* myPieces);

int p2p_download_start(char* filename);

/**********helper functions************/
int listenSock_setup(int port);
int connectToRemote(char* ip, int port);
int check_P2PMsg(char* buff);
int getPieceFromFile(char* piece, P2PMsg req);
int initMyPieces(pEntry myPieces);
pEntry* getMyPieces(char* name);
int setMyPieces(char* piece, int pNum);
int rmMyPieces(char* name);

/**********some temporate stuff for testing************/

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