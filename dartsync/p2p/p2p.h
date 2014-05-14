#ifndef DARTSYNCP2P
#define DARTSYNCP2P
#include "config.h"

// P2PInfo type
#define REQ 0
#define REQFIN 1
#define REREQ 2

// piece status
#define UNTOUCHED 0
#define DONE 1
#define DOWNLOADING 2
#define RETRY 3

// peer status
#define NOTUSING 0
#define USING 1

// control msg that handle p2p connection
typedef struct p2p_msg{
  int type;
  //the name of the file
  char name[FILE_NAME_LEN];
  // file szie
  int size;
  // the pieceNum in the file, start from 0
  int pNum;
  // port
  int srcPort;
  int destPort;
  // ip address
  char srcIp[NI_MAXHOST];
  char destIp[NI_MAXHOST];

}P2PInfo;

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
  // request info
  P2PInfo* req;
  // keep track of which one is done
  int* pieces;
  // keep track of which peer is using
  int* peers;
  // thread handlers
  pthread_t* threads;
  // the data area
  char* data;
  // pointer
  pEntry* next;
}pEntry;

void* p2p_listening(void* arg);

void* p2p_download(void* arg);

void* p2p_upload(void* arg);

int p2p_assemble(pEntry* myPieces);

void* p2p_download_start(void* arg);

/**********helper functions************/
// socket communication related functions
int listenSock_setup(int port);
int connectToRemote(char* ip, int port);
int check_P2PMsg(char* buff);
char* getMyIp();
int getAvailablePort();

// pTable related functions
int getPieceFromFile(char* piece, P2PInfo req);
int getPieceToDownload(int &pNum, pEntry* myPieces);
int initMyPieces(pEntry myPieces, P2PInfo* req);
int addMyPieces2pTable(pEntry myPieces);
pEntry* getMyPieces(char* name);
int writePieceData(pEntry* myPieces, char* piece, int pNum);
int rmMyPieces(pEntry* myPieces);

// thread related functions
int startDownThread(pEntry* myPieces, int &threadCnt, void* arg);
int startUpThread(pthread_t* tTable, int &threadCnt, void* arg);

// P2PInfo related
int initNewReq(P2PInfo* newReq, P2PInfo* req, int pNum, char* peerIP);

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

int getPeerIPFromFT(char* peerIP, char* filename);

#endif