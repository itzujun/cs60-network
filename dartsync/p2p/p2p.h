#ifndef DARTSYNCP2P
#define DARTSYNCP2P
#include <pthread.h>
#include "../common/constants.h"
#include "../common/filetable.h"
#include "../common/peertable.h"
#include "../common/pkt.h"

#define LISTEN_BACKLOG 50

// P2PInfo type
#define REQ 0
#define REQFIN 1
#define REREQ 2

// piece status
#define UNTOUCHED 0
#define P2PDONE 1
#define DOWNLOADING 2
#define RETRY 3

// peer status
#define NOTUSING 0
#define USING 1

typedef struct timespec TS;

// control msg that handle p2p connection
typedef struct p2p_msg{
  int type;
  //the name of the file
  char name[FILE_NAME_MAX_LEN];
  // file szie
  int size;
  // file timestamp
  unsigned long file_time_stamp;
  // the pieceNum in the file, start from 0
  int pNum;
  // port
  int srcPort;
  int destPort;
  // ip address
  char srcIP[IP_LEN];
  char destIP[IP_LEN];
  int sockfd;

}P2PInfo;

/**
pieces maintain table, 
*/
typedef struct p2p_pieceEntry{
  // filename
  char name[FILE_NAME_MAX_LEN];
  // size
  int size;
  // file timestamp
  unsigned long file_time_stamp;
  // count current pieces
  int piecesDone;
  // store total pieces
  int totalPieces;
  // request info
  P2PInfo* req;
  // keep track of which one is done
  int* pieces;
  // timestamp of each piece
  char** peerIPs;
  // timestamp of each piece
  TS* pTimes;
  // thread handlers
  pthread_t* threads;
  // lock
  pthread_mutex_t *mutex;
  // the data area
  char* data;
  // pointer
  struct p2p_pieceEntry* next;
  // restart flag
  int restart;
  // sigint to terminate
  int sigint;
  // piece cnt
  int pCnt;
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
int getMyIp(char* myIP);
int getAvailablePort(int *sock, int *port);

// pTable related functions
int getPieceFromFile(char* piece, P2PInfo* req);
int getPieceToTransfer(int *pNum, pEntry* myPieces);
int initMyPieces(pEntry* myPieces, P2PInfo* req);
int addMyPieces2pTable(pEntry* myPieces);
pEntry* getMyPieces(char* name);
int writePieceData(pEntry* myPieces, char* piece, int pNum);
int rmMyPieces(pEntry* myPieces);
int terminateMyPieces(pEntry* myPieces);
int  handleUpdateOrDelete(pEntry* myPieces);
int getIPFromFTE(char* peerIP, pEntry* myPieces, Node* fte);
int peer_use(pEntry* myPieces, char* ip);
int peer_rm(pEntry* myPieces, char* ip);

// thread related functions
int startUpThread(pthread_t* tTable, int sockfd);
int startDownThread(pEntry* myPieces);

// P2PInfo related
int initNewReq(P2PInfo* newReq, P2PInfo* req, int pNum, char* peerIP, char* myIP);



#endif
