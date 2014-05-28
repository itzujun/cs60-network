#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <ifaddrs.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "p2p.h"
#define MAXLINE 100
#define _GNU_SOURCE             /* See feature_test_macros(7) */

int upthreadCnt = 0;
int EXIT_SIG = 0;
pthread_t downThreads[MAX_THREAD_NUM];
pthread_t upThreads[MAX_THREAD_NUM];
int downloadPort = P2P_DOWNLOAD_PORT;
pEntry* pTable;
pthread_mutex_t *ptable_mutex;
extern int download_task;
Node*fte;

/**
steps:
  create server side uploadSock, listening and binding;
  run the accept function that watis for p2p_download thread;
  create p2p_upload thread with coressponding file, peiceSeqNum and remote specified port;
*/
void* p2p_listening(void* arg) {
  pthread_detach(pthread_self());
	ptable_mutex = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(ptable_mutex, NULL);

  int listenSock = listenSock_setup(P2P_LISTENING_PORT);
  int reqSock;
  struct sockaddr_in client_addr;
  socklen_t length = sizeof(client_addr);
  
  if(listenSock < 0) {
    fprintf(stderr, "--err in file %s func %s: \n----listenSock_setup on port %d fails!\n"
    , __FILE__, __func__, P2P_LISTENING_PORT); 
    return NULL;
  }

// handling new coming request of piece
  bzero(&upThreads, sizeof(pthread_t) * MAX_THREAD_NUM);
  printf("%s:\t\t START LISTENING...\n", __func__);
  while(!EXIT_SIG) {
      reqSock = accept(listenSock, (struct sockaddr*) &client_addr, &length);
      if (reqSock < 0) {
        fprintf(stderr, "--err in file %s func %s: \n----server accept failed!\n"
          , __FILE__, __func__);
        return NULL;
      }
      pthread_t newThread;
      pthread_create(&newThread, NULL, (void *) p2p_upload, (void *) &reqSock);
      printf("start p2p_upload thread for sockfd %d\n",reqSock);
  }
  return NULL;
}

/**
steps:
  connect to p2p_upload using specific port;
  create a server downloadSock and start listening;
  send the piece request and the randomly generated port;
  recv data from p2p_upload;
  if recv is finished, send fin signal to remote p2p_upload;
  close connection;
  send handshake to server use the file_monitor module built in function;
*/
void* p2p_download(void* arg) {
  pthread_detach(pthread_self());
  pEntry* myPieces = (pEntry*)arg;
  P2PInfo* req = myPieces->req;
  char* piece = malloc(PIECE_SIZE);
  int reqSock;
  
  printf("%s:\t\t DOWNLOAD THREAD FOR FILE %s PIECE %d WITH IP %s START\n", __func__, req->name, req->pNum, req->destIP);
  if((reqSock = connectToRemote(req->destIP, req->destPort)) < 0) {
    fprintf(stderr, "--err in file %s func %s: \n----connectToRemote fail.\n"
      , __FILE__, __func__); 
    close(reqSock);
    myPieces->pieces[req->pNum] = UNTOUCHED;
    //peer_peertable_rm(req->destIP, req->name);
    peer_rm(myPieces, req->destIP);
    return NULL;
  }
  
  /*printf("%s:\t\t FILE %s PIECE %d WITH IP %s WAIT FOR READY SIG\n", __func__, req->name, req->pNum, req->destIP);
  if ( (p2pRecvPkt(reqSock, msg, 128) ) < 0) {
    fprintf(stderr, "--err in file %s func %s: \n----send request of file %s piece %d to %s fail.\n"
      , __FILE__, __func__, req->name, req->pNum, req->destIP); 
    close(reqSock);
    myPieces->pieces[req->pNum] = UNTOUCHED;
    peer_peertable_rm(req->destIP, req->name);
    return NULL;
  }
  if( (strcmp(msg, "READY")) == 0)
    printf("%s:\t\t DOWNLOAD THREAD FOR FILE %s PIECE %d WITH IP %s READY SIGNAL GET\n", __func__, req->name, req->pNum, req->destIP);
*/

  if ( (send(reqSock, (char*)req, sizeof(P2PInfo), 0) ) < 0) {
    fprintf(stderr, "--err in file %s func %s: \n----send request of file %s piece %d to %s fail.\n"
      , __FILE__, __func__, req->name, req->pNum, req->destIP); 
    close(reqSock);
    myPieces->pieces[req->pNum] = UNTOUCHED;
    //peer_peertable_rm(req->destIP, req->name);
    peer_rm(myPieces, req->destIP);
    return NULL;
  }

  printf("%s:\t\t REQ FOR FILE %s PIECE %d TO %s SENT\n", __func__, req->name, req->pNum, req->destIP);
  //close(reqSock);
/*
  if ((downloadSock = accept(listenSock, (struct sockaddr*) &client_addr, &length)) < 0) {
    fprintf(stderr, "--err in file %s func %s: \n----downloader accept failed!\n"
      , __FILE__, __func__);
    return NULL;
  }
*/
  

  int pieceLen = PIECE_SIZE;
  if((req->pNum + 1) * PIECE_SIZE > req->size)
    pieceLen = req->size - (req->pNum) * PIECE_SIZE;

  if((recv(reqSock, piece, pieceLen, 0) )< 0) {
    fprintf(stderr, "--err in file %s func %s: \n----recv data name %s piece %d from %s fail.\n"
      , __FILE__, __func__, req->name, req->pNum, req->destIP);
    close(reqSock);
    myPieces->pieces[req->pNum] = UNTOUCHED;
    //peer_peertable_rm(req->destIP, req->name);
    peer_rm(myPieces, req->destIP);
    return NULL;
  }
  printf("%s:\t\t DOWNLOAD THREAD FOR FILE %s PIECE %d\n", __func__, req->name, req->pNum);

  /*
  req->type = REQFIN;
  if (send(downloadSock, req, sizeof(P2PInfo), 0) < 0) {
    fprintf(stderr, "--err in file %s func %s: \n----send to %s fail.\n"
      , __FILE__, __func__, req->srcIP); 
    close(downloadSock);
    return NULL;
  }
*/
  peer_rm(myPieces, req->destIP);
  //peer_peertable_rm(req->destIP, req->name);
  writePieceData(myPieces, piece, req->pNum);
  myPieces->pieces[req->pNum] = P2PDONE;
  pthread_mutex_lock(myPieces->mutex);
  myPieces->piecesDone++;
  pthread_mutex_unlock(myPieces->mutex);
  
  printf("%s:\t\t DOWNLOAD THREAD FOR FILE %s PIECE %d END, %d/%d DONE\n", __func__, req->name, req->pNum, myPieces->piecesDone, myPieces->totalPieces);
    
  return NULL;
}

/**
steps:
  add new pieces to pTable;
  get piece and peer to download
*/
void* p2p_download_start(void* arg) {
  pthread_detach(pthread_self());
  fte = (Node*)arg;
  P2PInfo* req = (P2PInfo*)malloc(sizeof(P2PInfo));
  req->size = fte->size;
  memcpy(req->name, fte->name, FILE_NAME_MAX_LEN);
  pEntry* myPieces = (pEntry*)malloc(sizeof(pEntry));
  //char peerIP[IP_LEN], myIp[IP_LEN];
  char* peerIP = malloc(IP_LEN);
  char* myIp = malloc(IP_LEN);

  int pNum;
  bzero(&downThreads, sizeof(pthread_t) * MAX_THREAD_NUM);

// creat a new piece, return the piece pointer
  //handleUpdateOrDelete(myPieces);
  if(initMyPieces(myPieces, req) < 0){
      fprintf(stderr, "--warning in file %s func %s: \n----some one is doing something on file %s, try next time.\n"
        , __FILE__, __func__, req->name); 
      return NULL;
  }
  
  printf("%s:\t\t FILE %s START TO DOWNLOAD>.>.>.>.>.>.>.>.>.>.>.>.>.>.>.>.>.>.>.>.>.>.>.>.>.>.\n", __func__, myPieces->name);
// start multi thread downloading
  while(!EXIT_SIG && myPieces->sigint != 1 && myPieces->piecesDone < myPieces->totalPieces ) {
  // @TODO: tell didi to implement it, 
  // get the next peer that is available, 
  // if no peer avaiable, sleep and wait
    
    if( getPieceToTransfer(&pNum, myPieces) < 0){
      //printf("%s:\t\t NO PIECE AVAILABLE, WAIT %ds\n", __func__, WAIT_PIECE_INTERVAL);
      
      sleep(WAIT_PIECE_INTERVAL);
      continue;
    }
    printf("%s:\t\t FILE %s GET pNUM %d\n", __func__, myPieces->name, pNum);
    
    if(getIPFromFTE(peerIP, myPieces, fte) < 0){
      printf("%s:\t\t NO IP FOR FILE %s pNUM %d:\n", __func__, myPieces->name, pNum);
      sleep(WAIT_PEER_INTERVAL);
      continue;
    }
    memcpy(myPieces->peerIPs[pNum], peerIP, IP_LEN);
        
    printf("%s:\t\t FILE %s GET IP %s\n", __func__, myPieces->name, peerIP);
    //peer_peertable_add(peerIP, req->name, req->file_time_stamp, -1);

    getMyIp(myIp);
    if(myIp == NULL) {
      //printf("%s:\t\t LOCAL NETWORK DOWN, WAIT %ds\n", __func__, WAIT_NET_UP_INTERVAL);
      sleep(WAIT_NET_UP_INTERVAL);
      continue;
    }
    
    P2PInfo* newReq = myPieces->req;
    bzero(newReq, sizeof(P2PInfo));
    initNewReq(newReq, req, pNum, peerIP, myIp);

    if((startDownThread(myPieces)) < 0) {
      fprintf(stderr, "--warning in file %s func %s: \n----no avaiable thread slots.\n"
        , __FILE__, __func__); 
      myPieces->pieces[pNum] = UNTOUCHED;
      peer_rm(myPieces, req->destIP);
      sleep(WAIT_THREAD_INTERVAL);
      continue;
    }

    printf("%s:\t\t %d/%d PIECES DONE FOR FILE %s!\n", __func__, myPieces->piecesDone, myPieces->totalPieces, myPieces->name);
  }
  
  if(myPieces->sigint == 1) {
  // if it is terminated, handle it
    terminateMyPieces(myPieces);
    printf("%s:\t\t FILE %s TERMINATE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n", __func__, myPieces->name);
  } else if(p2p_assemble(myPieces)) {
  // if finished assemble it
  // @TODO: trigger the file monitor handshake
    printf("%s:\t\t FILE %s GET!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n", __func__, myPieces->name);
  }
  rmMyPieces(myPieces);
  
  //free(fte);
  free(req);
  
  return NULL;
}

/**
steps:
  try to connect to remote p2p_download thread;
  get piece;
  send the data;
  wait for file fin signal;
*/
void* p2p_upload(void* arg) {
  pthread_detach(pthread_self());
  int* uploadSock = (int*)arg;
  char * piece = malloc(PIECE_SIZE);
  bzero(piece,PIECE_SIZE);
  P2PInfo* req = (P2PInfo*)malloc(sizeof(P2PInfo));
  bzero(req,sizeof(P2PInfo));

    /*strcpy(msg, "READY");
    if((p2pSendPkt(*uploadSock, msg, 128) )< 0) {
      fprintf(stderr, "--warning in file %s func %s: \n----recv fail for file %s piece %d.\n"
        , __FILE__, __func__, req->name, req->pNum);
      return NULL;
    }*/
    
    printf("%s:\t\t READY TO GET REQ\n", __func__);
    if((recv(*uploadSock, (char*)req, sizeof(P2PInfo), 0) ) < 0) {
      fprintf(stderr, "--warning in file %s func %s: \n----recv fail for file %s piece %d.\n"
        , __FILE__, __func__, req->name, req->pNum);
      return NULL;
    }

    printf("%s:\t\t GET REQUEST FOR FILE %s PIECE %d\n", __func__, req->name, req->pNum);

    if(check_P2PMsg((char*)req) < 0) {
      fprintf(stderr, "--warning in file %s func %s: \n----P2PInfo instead of junk expected.\n"
        , __FILE__, __func__); 
      return NULL;
    } 

  /*if((uploadSock = connectToRemote(req->srcIP, req->srcPort)) < 0) {
    fprintf(stderr, "--err in file %s func %s: \n----connectToRemote fail.\n"
      , __FILE__, __func__); 
    close(uploadSock);
    return NULL;
  }*/

  // @TODO: maybe a locker from file monitor
  if(getPieceFromFile(piece, req) < 0){
    fprintf(stderr, "--err in file %s func %s: \n----getPieceFromFile %s fail.\n"
      , __FILE__, __func__, req->name); 
    close(*uploadSock);
    return NULL;
  }
  printf("get file piece %d\n", req->pNum);
  //printf("%s:\t\t GET FILE %s PIECE %d TO SEND:\n%s\n", __func__, req->name, req->pNum, piece);

  int pieceLen = PIECE_SIZE;
  if((req->pNum + 1) * PIECE_SIZE > req->size)
    pieceLen = req->size - (req->pNum) * PIECE_SIZE;

  if( (send(*uploadSock, piece, pieceLen, 0)) < 0) {
    fprintf(stderr, "--err in file %s func %s: \n----send file %s piece %d to %s fail.\n"
      , __FILE__, __func__, req->name, req->pNum, req->srcIP); 
    close(*uploadSock);
    return NULL;
  }


/*
  if(recv(uploadSock, recvBuff, sizeof(P2PInfo), 0) < 0) {
    fprintf(stderr, "--err in file %s func %s: \n----recv from %s fail.\n"
      , __FILE__, __func__, req->srcIP);
    close(uploadSock);
    return NULL;
  }
  if(check_P2PMsg(recvBuff) < 0) {
    fprintf(stderr, "--warning in file %s func %s: \n----P2PInfo instead of junk expected.\n"
      , __FILE__, __func__); 
    close(uploadSock);
    return NULL;
  }
  if(((P2PInfo*)recvBuff)->type != REQFIN) {
    fprintf(stderr, "--warning in file %s func %s: \n----P2PInfo type %d err.\n"
      , __FILE__, __func__, ((P2PInfo*)recvBuff)->type); 
    close(uploadSock);
    return NULL;
  }
*/

  close(*uploadSock);
  return NULL;
}

/**
steps:
  create an output file pointer;
  write file;
  rm this node from pTable
*/
int p2p_assemble(pEntry* myPieces) {
  char tmpfilename[1024], number[5];
  FILE *ofp, *ifp;
  int i, size;

  /*pthread_mutex_lock(myPieces->mutex);
  myPieces->cnt++;
  pthread_mutex_unlock(myPieces->mutex);
  
  if(myPieces->cnt > 1){
    fprintf(stderr, "--err in file %s func %s: \n---- should not assemble file %s for the %dth time.\n"
        , __FILE__, __func__, myPieces->name, myPieces->cnt);
    return -1;
  }*/

  myPieces->data = (char*)malloc(myPieces->size);
  bzero(myPieces->data, myPieces->size);
  
  for(i = 0; i < myPieces->totalPieces; i++){
    size = (i == myPieces->totalPieces - 1) ? 
      myPieces->size % PIECE_SIZE : PIECE_SIZE;
    sprintf(number,"%d",i);
    bzero(tmpfilename, 1024);
    strcat(tmpfilename, myPieces->name);
    strcat(tmpfilename, ".");
    strcat(tmpfilename, number);
    strcat(tmpfilename, ".tmp");
  
    while(!EXIT_SIG && (ifp = fopen(tmpfilename, "r")) == NULL) {
    fprintf(stderr, "--err in file %s func %s: \n----open file %s fail with code %d: %s, retry %d later.\n"
        , __FILE__, __func__, myPieces->name, errno, strerror(errno), OPEN_FILE_TIMEOUT);
      sleep(OPEN_FILE_TIMEOUT);
    }

    if(fread (myPieces->data + PIECE_SIZE * i , sizeof(char), size, ifp) != size) {
      fprintf(stderr, "--err in file %s func %s: \n----read file %s error.\n"
        , __FILE__, __func__, tmpfilename);
      fclose (ifp);
      return -1;
    }

    fclose (ifp);
    if(remove(tmpfilename)){
      printf("%s:\t\t FILE %s PIECE %d ASSEMBLED\n", __func__, myPieces->name, i);
    }
    printf("%s:\t\t FILE %s PIECE %d ASSEMBLED\n", __func__, myPieces->name, i);
  }

  while(!EXIT_SIG && (ofp = fopen(myPieces->name, "w")) == NULL) {
    fprintf(stderr, "--err in file %s func %s: \n----open file %s fail with code %d: %s, retry %d later.\n"
        , __FILE__, __func__, myPieces->name, errno, strerror(errno), OPEN_FILE_TIMEOUT);
    sleep(OPEN_FILE_TIMEOUT);
  }

  if(fwrite (myPieces->data , sizeof(char), myPieces->size, ofp) != myPieces->size) {
    fprintf(stderr, "--err in file %s func %s: \n----write file %s error.\n"
      , __FILE__, __func__, myPieces->name);
    fclose (ofp);
    return -1;
  }
  
  fclose (ofp);
  //printf("%s:\t\t GET FILE CONTENT: %s\n", __func__, myPieces->data);

  Node* pn = findFileEntryByName(myPieces->name);
  if(pn != NULL){
  	pthread_mutex_lock(file_table_mutex);
	  pn->status = DONE;
  	pthread_mutex_unlock(file_table_mutex);
  }

  printf("%s:\t\t ASSEMBLE FILE %s DONE\n", __func__, myPieces->name);
  return 1;
}

/****************************************************
******************helper functions*******************
*****************************************************/

int terminateMyPieces(pEntry* myPieces) {
  /*int i;//, totalPieces;
  //char number[5], tmpfilename[1024];
  for(i = 0; i < myPieces->totalPieces; i++) {
    printf("%s:\t\t TERMINATING %d PIECE\n", __func__, i);
    pthread_join((pthread_t)(myPieces->threads + i), NULL);
  }*/
  // current design: not remove those tmp files;
  /*
  for(i = 0; i < totalPieces; i++) {
    sprintf(number,"%d",i);
    strcat(tmpfilename, myPieces->name);
    strcat(tmpfilename, ".");
    strcat(tmpfilename, number);
    strcat(tmpfilename, ".tmp");
    remove(tmpfilename);
  }*/
  printf("%s:\t\t TERMINATING FILE DOWNLOAD %s\n", __func__, myPieces->name);
  return 1;
}

// handle situation where new status come while downloading
int  handleUpdateOrDelete(pEntry* myPieces) {
  return 1;
}

int getAvailablePort(int *sock, int *port) {
  int cnt = 0, downloadSock;
  while(!EXIT_SIG && cnt < (P2P_DOWNLOAD_PORT_MAX - P2P_DOWNLOAD_PORT) 
    && (downloadSock = listenSock_setup(downloadPort)) < 0 ) {
    downloadPort ++;
    if(downloadPort >= P2P_DOWNLOAD_PORT_MAX){
      downloadPort = P2P_DOWNLOAD_PORT;

    }
    cnt++;
  }
  if(cnt >= (P2P_DOWNLOAD_PORT_MAX - P2P_DOWNLOAD_PORT) ) {
    return -1;
  }
  else{
    *port = downloadPort;
    *sock = downloadSock;
    downloadPort++;
    return 1;
  }
}

int initMyPieces(pEntry* myPieces, P2PInfo* req) {
  int i, totalPieces;
  char number[5], tmpfilename[1024];
  strcpy(myPieces->name, req->name);
  myPieces->size = req->size;
  myPieces->piecesDone = 0;
  myPieces->totalPieces = (req->size % PIECE_SIZE == 0) ? 
    req->size / PIECE_SIZE : req->size / PIECE_SIZE + 1;
  totalPieces = myPieces->totalPieces;
  myPieces->req =  (P2PInfo*)malloc(sizeof(P2PInfo));
  memcpy(myPieces->req, req, sizeof(P2PInfo));
  myPieces->pieces = (int*)malloc(sizeof(int) * totalPieces);
  myPieces->pTimes = (TS*)malloc(sizeof(TS) * totalPieces);  
  myPieces->threads = (pthread_t*)malloc(sizeof(pthread_t) * totalPieces);
	myPieces->mutex = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(myPieces->mutex, NULL);
  myPieces->next = NULL;
  myPieces->restart = 0;
  myPieces->sigint = 0;
  myPieces->pCnt = -1;
  myPieces->peerIPs = (char**) malloc(sizeof(char*) * totalPieces);
  for(i = 0; i < totalPieces; i++) {
    myPieces->peerIPs[i] = (char*)malloc(IP_LEN);
    myPieces->pieces[i] = UNTOUCHED;
  }

  if(addMyPieces2pTable(myPieces) < 0)
    return -1;
  // recontinue from partial download
  if(myPieces->restart == 1) {
    sleep(PDOWN_TIME_OUT);
    for(i = 0; i < totalPieces; i++) {
      sprintf(number,"%d",i);
      strcat(tmpfilename, myPieces->name);
      strcat(tmpfilename, ".");
      strcat(tmpfilename, number);
      strcat(tmpfilename, ".tmp");
      if( (access (tmpfilename, F_OK)) == 0 )
        myPieces->pieces[i] = DONE;
    }
  }
  
  printf("%s:\t\t MYPIECES CREATED FOR SYNC, NAME %s, SIZE %dB, %d PIECES\n", __func__, myPieces->name, myPieces->size, myPieces->totalPieces);
  return 1;
}

int addMyPieces2pTable(pEntry* myPieces) {
  pthread_mutex_lock(ptable_mutex);
  pEntry* pPtr = pTable;
  if(pPtr == NULL) {
    pTable = myPieces;
  } else {
    while(pPtr->next != NULL) {
      if(strcmp(pPtr->name, myPieces->name) == 0){
        //pPtr->sigint = 1;
        //myPieces->restart = 1;
        return -1;
      }
      pPtr = pPtr->next;
    }
    if(strcmp(pPtr->name, myPieces->name) == 0){
      //pPtr->sigint = 1;
      //myPieces->restart = 1;
      return -1;
    }
    pPtr->next = myPieces;
  }
  pthread_mutex_unlock(ptable_mutex);
  return 1;
}

pEntry* getMyPieces(char* name) {
  pthread_mutex_lock(ptable_mutex);
  pEntry* pPtr = pTable;
  while(pPtr) {
    if(strcmp(pPtr->name, name) == 0)
      break;
    pPtr = pPtr->next;
  }
  pthread_mutex_unlock(ptable_mutex);
  return pPtr;
}

int rmMyPieces(pEntry* myPieces) {
  pEntry* pPtr = pTable;

  if(pTable == NULL) {
    fprintf(stderr, "--err in file %s func %s: \n----pTable is NULL.\n"
      , __FILE__, __func__);
    return -1;
  }

  if(pPtr == myPieces) {
    pTable = pPtr->next;
  } else {
    while(pPtr->next != myPieces) {
      if((pPtr = pPtr->next) == NULL) {
        fprintf(stderr, "--err in file %s func %s: \n----myPieces not found.\n"
          , __FILE__, __func__);
        return -1;
      }
    }
    pPtr->next = pPtr->next->next;
  }
  free(myPieces->req);
  free(myPieces->pieces);
  free(myPieces->threads);
  free(myPieces->data);
  free(myPieces);
  myPieces = NULL;
  return 1;
}

int writePieceData(pEntry* myPieces, char* piece, int pNum) {
  int size = (pNum == myPieces->totalPieces - 1) ? 
    myPieces->size % PIECE_SIZE : PIECE_SIZE;
  FILE *ofp;
  char tmpfilename[1024], number[5];
  sprintf(number, "%d", pNum);
  strcat(tmpfilename, myPieces->name);
  strcat(tmpfilename, ".");
  strcat(tmpfilename, number);
  strcat(tmpfilename, ".tmp");

  while(!EXIT_SIG && (ofp = fopen(tmpfilename, "wb")) == NULL) {
    fprintf(stderr, "--err in file %s func %s: \n----open file %s fail with code %d: %s, retry %d later.\n"
        , __FILE__, __func__, myPieces->name, errno, strerror(errno), OPEN_FILE_TIMEOUT);
    sleep(OPEN_FILE_TIMEOUT);
  }

  if(fwrite (piece , sizeof(char), size, ofp) != size) {
    fprintf(stderr, "--err in file %s func %s: \n----write file %s error.\n"
      , __FILE__, __func__, tmpfilename);
    fclose (ofp);
    return -1;
  }
  fclose (ofp);
  return 1;
}



int startDownThread(pEntry* myPieces) {
  P2PInfo* req = myPieces->req;
  if (pthread_create((myPieces->threads + req->pNum), NULL, (void *) p2p_download, (void *) myPieces) != 0) {
    fprintf(stderr, "--warning in file %s func %s: \n----%dth piece create thread fail.\n"
      , __FILE__, __func__, req->pNum);
    return -1;
  }
  //printf("start p2p_download thread for pNum %d\t \n",req->pNum );
  return 1;
}

int connectToRemote(char* ip, int port){
  int sock;
  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == -1) {
    fprintf(stderr, "--err in file %s func %s: \n----could not create sock.\n"
      , __FILE__, __func__);
  }
  struct sockaddr_in server;

  server.sin_addr.s_addr = inet_addr(ip);
  server.sin_family = AF_INET;
  server.sin_port = htons(port);
  if (connect(sock, (struct sockaddr *) &server, sizeof(server)) < 0) 
    return -1;
  else
    return sock;
}

// return -1 if fail
int listenSock_setup(int port) {
  // build server Socket
  int server_socket = socket(PF_INET, SOCK_STREAM, 0);
  if (server_socket < 0) {

    return -1;
  } else {
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  }

  // set server configuration
  struct sockaddr_in server_addr;
  bzero(&server_addr, sizeof(server_addr));

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htons(INADDR_ANY);
  server_addr.sin_port = htons(port);

  // bind the port
  if (bind(server_socket, (struct sockaddr*) &server_addr,
    sizeof(server_addr))<0) {
    //fprintf(stderr, "--err in file %s func %s: \n----bind port %d failed, try another one.\n"
      //, __FILE__, __func__, port); 
    return -1;
  }

    // listening
  if (listen(server_socket, LISTEN_BACKLOG) <0) {
    fprintf(stderr, "--err in file %s func %s: \n----Bind port %d failed.\n"
      , __FILE__, __func__, port); 
    return -1;
  }

  return server_socket;
}

int getPieceFromFile(char* piece, P2PInfo* req) {
  int file = 0;
  int pieceLen, getLen;

  if((file=open(req->name, O_RDONLY)) < 0){
    fprintf(stderr, "--err in file %s func %s: \n----open file %s fails.\n"
      , __FILE__, __func__, req->name); 
    return -1;
  }

  pieceLen = PIECE_SIZE;
  if((req->pNum + 1) * PIECE_SIZE > req->size)
    pieceLen = req->size - (req->pNum) * PIECE_SIZE;

  if(lseek(file, req->pNum * PIECE_SIZE, SEEK_SET) < 0) {
    fprintf(stderr, "--err in file %s func %s: \n----piece %d not found.\n"
      , __FILE__, __func__, req->pNum); 
    close(file);
    return -1;
  }

  if((getLen = read(file, piece, pieceLen)) != pieceLen) {
    fprintf(stderr, "--err in file %s func %s: \n----get piece size %d, while %d requested.\n"
      , __FILE__, __func__, pieceLen, getLen); 
    close(file);
    return -1;
  }

  close(file);
  return 1;
}

int getPieceToTransfer(int *pNum, pEntry* myPieces) {
  int idx, i;
  TS curr = {0, 0};
  clock_gettime(CLOCK_MONOTONIC, &curr);
  pthread_mutex_lock(myPieces->mutex);
  myPieces->pCnt = (myPieces->pCnt + 1) % myPieces->totalPieces;
  pthread_mutex_unlock(myPieces->mutex);
  for(i = 0; i < myPieces->totalPieces; i++) {
    idx = (myPieces->pCnt + i) % myPieces->totalPieces;
    if(myPieces->pieces[idx] == UNTOUCHED ) {
      *pNum = idx;
      myPieces->pieces[*pNum] = DOWNLOADING;
      myPieces->pTimes[*pNum] = curr;
      return 1;
    }
    
    //printf("%s:\t\t TIME DURATION %ds WITH STAT%d, pNUm %d\n", __func__, (int)( curr.tv_sec - myPieces->pTimes[idx].tv_sec), myPieces->pieces[idx], idx);
    if(myPieces->pieces[idx] == DOWNLOADING 
      && (int)( curr.tv_sec - myPieces->pTimes[idx].tv_sec) > PDOWN_TIME_OUT ) {
      //pthread_cancel(myPieces->threads[*pNum]);
      myPieces->pTimes[idx] = curr;
      memset(myPieces->peerIPs[idx], 0, IP_LEN);
      *pNum = idx;
      return 1;
    }
    // although it is downloading, the thread is terminated
    // @TODO: pthread_tryjoin_np implicit declaration, need other way to deal with re-continue
    // if(myPieces->pieces[idx] == DOWNLOADING 
    //   && (pthread_tryjoin_np(myPieces->threads[idx], NULL) == 0)) {
    //   *pNum = idx;
    //   return 1;
    // }
  }
  return -1;
}

unsigned long getSysTime(TS time){
	return 0;
}

int initNewReq(P2PInfo* newReq, P2PInfo* req, int pNum, char* peerIP, char* myIP) {
  (newReq)->type = REQ;
  memcpy((newReq)->name, req->name, FILE_NAME_MAX_LEN);
  (newReq)->size = req->size;
  (newReq)->pNum = pNum;
  (newReq)->srcPort = 0; // will be initiated later
  (newReq)->destPort = P2P_LISTENING_PORT; 
  strcpy((newReq)->srcIP, myIP);
  strcpy((newReq)->destIP, peerIP);
  if((newReq)->srcIP == NULL)
    return -1;
  else
    return 1;
}

// reference: http://stackoverflow.com/questions/2283494/get-ip-address-of-an-interface-on-linux
// reference: http://man7.org/linux/man-pages/man3/getifaddrs.3.html
int getMyIp(char *myip) {
  char* hostname = malloc(MAXLINE);
  bzero(hostname,MAXLINE);
  if(gethostname(hostname, MAXLINE) <0){
    perror("gethostname");
    return -1;
  }
  char*ip = NULL;
  struct hostent* h =NULL;
  if((h=gethostbyname(hostname))==NULL){
    perror("gethostbyname");
    return -1;
  }
  if((ip = inet_ntoa(*((struct in_addr*)h->h_addr_list[0])))== NULL){
    perror("addr_ntoa");
    return -1;
  }
  strcpy(myip, ip);
  return 0;
}
// int getMyIp(char* host) {
//   // int fd;
//   // struct ifreq ifr;

//   // fd = socket(AF_INET, SOCK_DGRAM, 0);

//   // /* I want to get an IPv4 IP address */
//   // ifr.ifr_addr.sa_family = AF_INET;

//   //  I want IP address attached to "eth0" 
//   // strncpy(ifr.ifr_name, "eth0", IFNAMSIZ-1);

//   // ioctl(fd, SIOCGIFADDR, &ifr);

//   // close(fd);

//   // /* display result */
//   // return inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
//   struct ifaddrs *ifaddr, *ifa;
//   int family, s, n;

//   if (getifaddrs(&ifaddr) == -1) {
//    perror("getifaddrs");
//    exit(EXIT_FAILURE);
//  }

//   /* Walk through linked list, maintaining head pointer so we
//     can free list later */

//  for (ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++) {
//    if (ifa->ifa_addr == NULL)
//      continue;

//    family = ifa->ifa_addr->sa_family;

//      /* Display interface name and family (including symbolic
//         form of the latter for the common families) */

//      // printf("%-8s %s (%d)\n",
//      //        ifa->ifa_name,
//      //        (family == AF_PACKET) ? "AF_PACKET" :
//      //        (family == AF_INET) ? "AF_INET" :
//      //        (family == AF_INET6) ? "AF_INET6" : "???",
//      //        family);

//      /* For an AF_INET* interface address, display the address */

//    if (family == AF_INET || family == AF_INET6) {
//      s = getnameinfo(ifa->ifa_addr,
//        (family == AF_INET) ? sizeof(struct sockaddr_in) :
//        sizeof(struct sockaddr_in6),
//        host, NI_MAXHOST,
//        NULL, 0, NI_NUMERICHOST);
//      if (s != 0) {
//        printf("getnameinfo() failed: %s\n", gai_strerror(s));
//        return 0;
//      }
//      if(strcmp("127.0.0.1", host) != 0 && strlen(host) <= 16){
//        freeifaddrs(ifaddr);
//        return 1;
//      }

//          // printf("\t\taddress: <%s>\n", host);

//    } else if (family == AF_PACKET && ifa->ifa_data != NULL) {
//      struct rtnl_link_stats *stats = ifa->ifa_data;

//          // printf("\t\ttx_packets = %10u; rx_packets = %10u\n"
//          //        "\t\ttx_bytes   = %10u; rx_bytes   = %10u\n",
//          //        stats->tx_packets, stats->rx_packets,
//          //        stats->tx_bytes, stats->rx_bytes);
//    }
//  }
//  freeifaddrs(ifaddr);
//  return -1;
// }

int check_P2PMsg(char* buff) {
  P2PInfo* req = (P2PInfo*)buff;
  if(strlen(req->name) == 0)
    return -1;
  else
    return 1;
}

int getIPFromFTE(char* peerIP, pEntry* myPieces, Node* fte) {
  Node* itr = fte;
  int i;
	for(i = 0; i < itr->peer_ip_num; i++ ) {
		if(peer_use(myPieces, itr->newpeerip[i]) < 0) {
			memcpy(peerIP, itr->newpeerip[i], IP_LEN);
			return 1;
		}
		//printf("%s:\t\t IP %s FOR FILE %s IS CURRENTLY IN USE\n", __func__, itr->newpeerip[i], filename);
	}
	return -1;
}

int peer_use(pEntry* myPieces, char* ip) {
  int i = 0;
  for(; i < myPieces->totalPieces; i++) {
    if(strcmp(myPieces->peerIPs[i], ip) == 0 )
      return 1;
  }
  return -1;
}

int peer_rm(pEntry* myPieces, char* ip) {
  int i = 0;
  for(; i < myPieces->totalPieces; i++) {
    if(strcmp(myPieces->peerIPs[i], ip) == 0 ){
      memset(myPieces->peerIPs[i], 0, IP_LEN);
      return 1;
    }
  }
  return -1;
}





