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
#include <pthread.h>

#define MAX_THREAD_NUM 99

int threadCnt = 0;
int EXIT_SIG = 0;
pthread_t threads[MAX_THREAD_NUM];
int downloadPort = P2P_DOWNLOAD_PORT;
pEntry* pTable;

/**
steps:
  create server side uploadSock, listening and binding;
  run the accept function that watis for p2p_download thread;
  create p2p_upload thread with coressponding file, peiceSeqNum and remote specified port;
*/
void* p2p_listening(void* arg) {
  int listenSock = listenSock_setup(P2P_LISTENING_PORT);
  char recvBuff[sizeof(P2PMsg)];
  struct sockaddr_in client_addr;
  socklen_t length = sizeof(client_addr);

  // handling new coming request of piece
  bzero(&threads, sizeof(pthread_t) * MAX_THREAD_NUM);
  while(!EXIT_SIG) {
    printf("%s:\t\t START LISTENING...\n", __func__);
    int reqSock = accept(server_socket, (struct sockaddr*) &client_addr, &length);
    if (reqSock < 0) {
      fprintf(stderr, "--err in file %s func %s: \n--server accept failed!\n"
        , __FILE__, __func__, port); 
      return -1;
    }

    if(recv(reqSock, recvBuff, sizeof(P2PMsg), 0) < 0) {
      fprintf(stderr, "--warning in file %s func %s: \n--recv fail.\n"
        , __FILE__, __func__); 
      continue;
    }
    
    if(check_P2PMsg(recvBuff) < 0) {
      fprintf(stderr, "--warning in file %s func %s: \n--P2PMsg instead of junk expected.\n"
        , __FILE__, __func__); 
      continue;
    } 

    printf("%s:\t\t REQUEST RECEVIED\n", __func__);
    if (pthread_create(threads + (threadCnt++), NULL,
        (void *) handle_clients, (void *) recvBuff) != 0) {
      fprintf(stderr, "--warning in file %s func %s: \n--creat %dth thread fail.\n"
        , __FILE__, __func__, threadCnt);
      continue;
    }

    printf("%s:\t\t UPLOAD THREAD CREATED\n", __func__);
  }

  close(reqSock);
  return;
}

/**
steps:
  connect to p2p_upload using specific port;
  send the piece request and the randomly generated port;
  create a server downloadSock and start listening;
  recv data from p2p_upload;
  if recv is finished, send fin signal to remote p2p_upload;
  close connection;
  send handshake to server use the file_monitor module built in function;
*/
void* p2p_download(void* arg) {
  P2PMsg* req = (P2PMsg*)arg;
  char piece[PIECE_SIZE];
  int downloadSock;
  int reqSock;

  if((reqSock = connectToRemote(req->destIp, req->destPort)) < 0) {
    fprintf(stderr, "--err in file %s func %s: \n--connectToRemote fail.\n"
      , __FILE__, __func__); 
    close(reqSock);
    return;
  }

  if (send(reqSock, req, sizeof(P2PMsg), 0) < 0) {
    fprintf(stderr, "--err in file %s func %s: \n--send to %s fail.\n"
      , __FILE__, __func__, req->srcIp); 
    close(reqSock);
    return;
  }

  close(reqSock);

  while(!EXIT_SIG && (downloadSock = listenSock_setup(downloadPort++)) < 0 ) {
    if(downloadPort >= P2P_DOWNLOAD_PORT_MAX)
      downloadPort = P2P_DOWNLOAD_PORT;
  }

  if(recv(downloadSock, piece, PIECE_SIZE, 0) < 0) {
    fprintf(stderr, "--err in file %s func %s: \n--recv from %s fail.\n"
      , __FILE__, __func__, req->srcIp);
    close(downloadSock);
    return;
  }

  pEntry* myPieces = getMyPieces(req->name);
  setMyPieces(piece, req->pNum);
  myPieces->pieces[req->pNum] = 0;
  myPieces->piecesDone++;
  if(myPieces->piecesDone == myPieces->totalPieces) {
    if(p2p_assemble(myPieces)) {
      // @TODO: trigger the file monitor handshake
      printf("%s:\t\t FILE GOT, SEND UPDATE TO TRACKER\n", __func__);
    }
  }
  
  return;
}

/**
steps:
  add new pieces to pTable;
*/
int p2p_download_start(char* filename, int size) {
  pEntry* myPieces = (pEntry*)malloc(sizeof(pEntry));
  if(initMyPieces(myPieces) < 0) {
    fprintf(stderr, "--err in file %s func %s: \n--init download fail.\n"
      , __FILE__, __func__, req->srcIp);
    return -1;
  }
  pEntry* pPtr = pTable;

  if(pPtr == NULL) {
    pPtr = (pEntry*)malloc(sizeof(pEntry));
  } else {
    while(pPtr->next != NULL)
      pPtr = pPtr->next;
    pPtr->next = (pEntry*)malloc(sizeof(pEntry));
    pPtr = pPtr->next;
  }
}

/**
steps:
  try to connect to remote p2p_download thread;
  get piece;
  send the data;
  wait for file fin signal;
*/
void* p2p_upload(void* arg) {
  P2PMsg* req = (P2PMsg*)arg;
  char piece[PIECE_SIZE];
  char recvBuff[sizeof(P2PMsg)];
  int uploadSock;

  if((uploadSock = connectToRemote(req->srcIp, req->srcPort)) < 0) {
    fprintf(stderr, "--err in file %s func %s: \n--connectToRemote fail.\n"
      , __FILE__, __func__); 
    close(uploadSock);
    return;
  }

  // @TODO: maybe a locker from file monitor
  if(getPieceFromFile(piece, req) < 0){
    fprintf(stderr, "--err in file %s func %s: \n--getPieceFromFile %s fail.\n"
      , __FILE__, __func__, req->name); 
    close(uploadSock);
    return;
  }

  if(send(uploadSock, piece, PIECE_SIZE, 0) < 0) {
    fprintf(stderr, "--err in file %s func %s: \n--send to %s fail.\n"
      , __FILE__, __func__, req->srcIp); 
    close(uploadSock);
    return;
  }

  if(recv(uploadSock, recvBuff, sizeof(P2PMsg), 0) < 0) {
    fprintf(stderr, "--err in file %s func %s: \n--recv from %s fail.\n"
      , __FILE__, __func__, req->srcIp);
    close(uploadSock);
    return;
  }

  if(check_P2PMsg(recvBuff) < 0) {
    fprintf(stderr, "--warning in file %s func %s: \n--P2PMsg instead of junk expected.\n"
      , __FILE__, __func__); 
    close(uploadSock);
    return;
  }

  if((P2PMsg*)recvBuff->type != REQFIN) {
    fprintf(stderr, "--warning in file %s func %s: \n--P2PMsg type %d err.\n"
      , __FILE__, __func__, (P2PMsg*)recvBuff->type); 
    close(uploadSock);
    return;
  }

  close(uploadSock);
  return;
}

/**
steps:
  create an output file pointer;
  write file;
  rm this node from pTable
*/
int p2p_assemble(pEntry* myPieces) {
  int filedesc;

  while(!EXIT_SIG && (filedesc = open(myPieces->name, O_WRONLY | O_APPEND))) < 0) {
    fprintf(stderr, "--err in file %s func %s: \n--open file %s fail, retry %d later.\n"
      , __FILE__, __func__, myPieces->name, OPEN_FILE_TIMEOUT);
    sleep(OPEN_FILE_TIMEOUT);
  }

  if(write(filedesc, myPieces->data, myPieces->size) != myPieces->size) {
    fprintf(stderr, "--err in file %s func %s: \n--write file error.\n"
      , __FILE__, __func__, myPieces->name, OPEN_FILE_TIMEOUT);
      return -1;
  }

  return 1;
}

/**********helper functions************/
int connectToRemote(char* ip, int port){
  int uploadSock = create_socket();
  struct sockaddr_in server;

  server.sin_addr.s_addr = inet_addr(ip);
  server.sin_family = AF_INET;
  server.sin_port = htons(port);
  if (connect(uploadSock, (struct sockaddr *) &server, sizeof(server)) < 0) 
    return -1;
  else
    return uploadSock;
}

// return -1 if fail
int listenSock_setup(int port) {
  // build server Socket
  int server_socket = socket(PF_INET, SOCK_STREAM, 0);
  if (server_socket < 0) {
    fprintf(stderr, "--err in file %s func %s: \n--listen sock create failed.\n"
      , __FILE__, __func__, port); 
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
      sizeof(server_addr))) {
    fprintf(stderr, "--err in file %s func %s: \n--bind port %d failed, try another one.\n"
      , __FILE__, __func__, port); 
    return -1;
  }

  // listening
  if (listen(server_socket, LISTEN_QUEUE_LENGTH)) {
    fprintf(stderr, "--err in file %s func %s: \n--Bind port %d failed.\n"
      , __FILE__, __func__, port); 
    return -1;
  }

  return server_socket;
}

int getPieceFromFile(char* piece, P2PMsg req) {
  int file = 0;
  int pieceLen, getLen;

  if((file=open(piece->name, O_RDONLY)) < 0){
    fprintf(stderr, "--err in file %s func %s: \n--open file %s fails.\n"
      , __FILE__, __func__, piece->name); 
    return -1;
  }

  if((req->pNum + 1) * PIECE_SIZE > req->size)
    pieceLen = req->size - (req->pNum) * PIECE_SIZE;

  if(lseek(file, req->pNum * PIECE_SIZE, SEEK_SET) < 0) {
    fprintf(stderr, "--err in file %s func %s: \n--piece %d not found.\n"
      , __FILE__, __func__, req->pNum); 
    return -1;
  }

  if((getLen = read(file, piece, pieceLen)) != pieceLen) {
    fprintf(stderr, "--err in file %s func %s: \n--get piece size %d, while %d requested.\n"
      , __FILE__, __func__, pieceLen, getLen); 
    return -1;
  }

  return 1;
}