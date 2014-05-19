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
#include <ifaddrs.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "p2p.h"

int upthreadCnt = 0;
int EXIT_SIG = 0;
pthread_t downThreads[MAX_THREAD_NUM];
pthread_t upThreads[MAX_THREAD_NUM];
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
  int reqSock;
  char recvBuff[sizeof(P2PInfo)];
  struct sockaddr_in client_addr;
  socklen_t length = sizeof(client_addr);

// handling new coming request of piece
  bzero(&upThreads, sizeof(pthread_t) * MAX_THREAD_NUM);
  while(!EXIT_SIG) {
    printf("%s:\t\t START LISTENING...\n", __func__);
    reqSock = accept(listenSock, (struct sockaddr*) &client_addr, &length);
    if (reqSock < 0) {
      fprintf(stderr, "--err in file %s func %s: \n--server accept failed!\n"
        , __FILE__, __func__);
      return;
    }

    if(recv(reqSock, recvBuff, sizeof(P2PInfo), 0) < 0) {
      fprintf(stderr, "--warning in file %s func %s: \n--recv fail.\n"
        , __FILE__, __func__);
      continue;
    }

    if(check_P2PMsg(recvBuff) < 0) {
      fprintf(stderr, "--warning in file %s func %s: \n--P2PInfo instead of junk expected.\n"
        , __FILE__, __func__); 
      continue;
    } 

    printf("%s:\t\t REQUEST RECEVIED\n", __func__);
    if(startUpThread(upThreads, recvBuff) < 0) {
      fprintf(stderr, "--warning in file %s func %s: \n--no avaiable thread slots.\n"
        , __FILE__, __func__);
      continue;
    }
  // upthreadCnt = (upthreadCnt + 1) % MAX_THREAD_NUM;
  // if (pthread_create(upThreads + upthreadCnt), NULL,
  //     (void *) p2p_upload, (void *) recvBuff) != 0) {
  //   fprintf(stderr, "--warning in file %s func %s: \n--creat %dth thread fail.\n"
  //     , __FILE__, __func__, upthreadCnt);
  //   continue;
  // }

    printf("%s:\t\t UPLOAD THREAD CREATED\n", __func__);
  }

  close(reqSock);
  return;
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
  P2PInfo* req = (P2PInfo*)arg;
  pEntry* myPieces;
  char piece[PIECE_SIZE];
  int downloadSock, listenSock;
  int reqSock;
  struct sockaddr_in client_addr;
  socklen_t length = sizeof(client_addr);

  if((myPieces = getMyPieces(req->name)) == NULL) {
    fprintf(stderr, "--err in file %s func %s: \n--getMyPieces fail.\n"
      , __FILE__, __func__); 
    return;
  }

  // find a port that is available
  if(getAvailablePort(&listenSock, &req->srcPort) < 0) {
    fprintf(stderr, "--err in file %s func %s: \n--getAvailablePort fail.\n"
      , __FILE__, __func__); 
    return;
  }

  if((reqSock = connectToRemote(req->destIP, req->destPort)) < 0) {
    fprintf(stderr, "--err in file %s func %s: \n--connectToRemote fail.\n"
      , __FILE__, __func__); 
    close(reqSock);
    return;
  }

  if (send(reqSock, req, sizeof(P2PInfo), 0) < 0) {
    fprintf(stderr, "--err in file %s func %s: \n--send to %s fail.\n"
      , __FILE__, __func__, req->srcIP); 
    close(reqSock);
    return;
  }

  close(reqSock);

  peer_table_add(req->destIP, req->name, req->file_time_stamp, downloadSock, reqSock);

  if ((downloadSock = accept(listenSock, (struct sockaddr*) &client_addr, &length)) < 0) {
    fprintf(stderr, "--err in file %s func %s: \n--downloader accept failed!\n"
      , __FILE__, __func__);
    return;
  }

  if(recv(downloadSock, piece, PIECE_SIZE, 0) < 0) {
    fprintf(stderr, "--err in file %s func %s: \n--recv from %s fail.\n"
      , __FILE__, __func__, req->srcIP);
    close(downloadSock);
    peer_table_rm(req->destIP, req->name, req->file_time_stamp, downloadSock, reqSock);
    return;
  }

  req->type = REQFIN;
  if (send(downloadSock, req, sizeof(P2PInfo), 0) < 0) {
    fprintf(stderr, "--err in file %s func %s: \n--send to %s fail.\n"
      , __FILE__, __func__, req->srcIP); 
    close(downloadSock);
    return;
  }

  writePieceData(myPieces, piece, req->pNum);

  myPieces->pieces[req->pNum] = DONE;
  myPieces->piecesDone++;

  if(myPieces->piecesDone == myPieces->totalPieces) {
    if(p2p_assemble(myPieces)) {
    // @TODO: trigger the file monitor handshake
      printf("%s:\t\t FILE GOT, SEND UPDATE TO TRACKER\n", __func__);
    }
  }

  peer_table_rm(req->destIP, req->name, req->file_time_stamp, downloadSock, reqSock);
  free(req);
  return;
}

/**
steps:
  add new pieces to pTable;
  get piece and peer to download
*/
void* p2p_download_start(void* arg) {
  P2PInfo* req = (P2PInfo*)arg;
  P2PInfo* newReq;
  pEntry* myPieces = (pEntry*)malloc(sizeof(pEntry));
  char peerIP[NI_MAXHOST], myIp[NI_MAXHOST];
  int pNum;
  bzero(&downThreads, sizeof(pthread_t) * MAX_THREAD_NUM);

// creat a new piece, return the piece pointer
  initMyPieces(myPieces, req);
  addMyPieces2pTable(myPieces);
  // if( < 0) {
  //   fprintf(stderr, "--err in file %s func %s: \n--init download fail.\n"
  //     , __FILE__, __func__, req->srcIP);
  //   return -1;
  // }

// start multi thread downloading
  while(!EXIT_SIG && myPieces->piecesDone < myPieces->totalPieces) {
  // @TODO: tell didi to implement it, 
  // get the next peer that is available, 
  // if no peer avaiable, sleep and wait
    if(getPieceToTransfer(&pNum, myPieces) < 0){
      printf("%s:\t\t NO PIECE AVAILABLE, WAIT %ds\n", __func__, WAIT_PIECE_INTERVAL);
      sleep(WAIT_PIECE_INTERVAL);
      continue;
    }

    if(getPeerIPFromPT(peerIP, req->name) < 0){
    // no one is available currently
      printf("%s:\t\t NO PEER AVAILABLE, WAIT %ds\n", __func__, WAIT_PEER_INTERVAL);
      sleep(WAIT_PEER_INTERVAL);
      continue;
    }

    getMyIp(myIp);
    if(myIp == NULL) {
      printf("%s:\t\t LOCAL NETWORK DOWN, WAIT %ds\n", __func__, WAIT_NET_UP_INTERVAL);
      sleep(WAIT_NET_UP_INTERVAL);
      continue;
    }
    initNewReq(newReq, req, pNum, peerIP, myIp);

    if(startDownThread(myPieces, newReq) < 0) {
      fprintf(stderr, "--warning in file %s func %s: \n--no avaiable thread slots.\n"
        , __FILE__, __func__); 
      sleep(WAIT_THREAD_INTERVAL);
      continue;
    }
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
  P2PInfo* req = (P2PInfo*)arg;
  char piece[PIECE_SIZE];
  char recvBuff[sizeof(P2PInfo)];
  int uploadSock;

  if((uploadSock = connectToRemote(req->srcIP, req->srcPort)) < 0) {
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
      , __FILE__, __func__, req->srcIP); 
    close(uploadSock);
    return;
  }

  if(recv(uploadSock, recvBuff, sizeof(P2PInfo), 0) < 0) {
    fprintf(stderr, "--err in file %s func %s: \n--recv from %s fail.\n"
      , __FILE__, __func__, req->srcIP);
    close(uploadSock);
    return;
  }

  if(check_P2PMsg(recvBuff) < 0) {
    fprintf(stderr, "--warning in file %s func %s: \n--P2PInfo instead of junk expected.\n"
      , __FILE__, __func__); 
    close(uploadSock);
    return;
  }

  if(((P2PInfo*)recvBuff)->type != REQFIN) {
    fprintf(stderr, "--warning in file %s func %s: \n--P2PInfo type %d err.\n"
      , __FILE__, __func__, ((P2PInfo*)recvBuff)->type); 
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

  while(!EXIT_SIG && (filedesc = open(myPieces->name, O_WRONLY | O_APPEND)) < 0) {
    fprintf(stderr, "--err in file %s func %s: \n--open file %s fail, retry %d later.\n"
      , __FILE__, __func__, myPieces->name, OPEN_FILE_TIMEOUT);
    sleep(OPEN_FILE_TIMEOUT);
  }

  if(write(filedesc, myPieces->data, myPieces->size) != myPieces->size) {
    fprintf(stderr, "--err in file %s func %s: \n--write file %s error.\n"
      , __FILE__, __func__, myPieces->name);
    return -1;
  }

  rmMyPieces(myPieces);
  return 1;
}

/****************************************************
******************helper functions*******************
*****************************************************/

int getAvailablePort(int *sock, int *port) {
  int cnt = 0, downloadSock;
  while(!EXIT_SIG && cnt < (P2P_DOWNLOAD_PORT_MAX - P2P_DOWNLOAD_PORT) 
    && (downloadSock = listenSock_setup(downloadPort)) < 0 ) {
    if(downloadPort >= P2P_DOWNLOAD_PORT_MAX)
      downloadPort = P2P_DOWNLOAD_PORT;
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
  strcpy(myPieces->name, req->name);
  myPieces->size = req->size;
  myPieces->piecesDone = 0;
  myPieces->totalPieces = (req->size % PIECE_SIZE == 0) ? 
    req->size / PIECE_SIZE : req->size / PIECE_SIZE + 1;
  totalPieces = myPieces->totalPieces;
  memcpy(myPieces->req, req, sizeof(P2PInfo));
  myPieces->pieces = (int*)malloc(sizeof(int) * totalPieces);
  myPieces->peers = (int*)malloc(sizeof(int) * totalPieces);
  myPieces->threads = (pthread_t*)malloc(sizeof(pthread_t) * totalPieces);
  myPieces->data = (char*)malloc(sizeof(char) * totalPieces);
  myPieces->next = NULL;
  for(i = 0; i < totalPieces; i++) {
    myPieces->pieces[i] = UNTOUCHED;
    myPieces->peers[i] = NOTUSING;
  }
  return 1;
}

int addMyPieces2pTable(pEntry* myPieces) {
  pEntry* pPtr = pTable;
  if(pPtr == NULL) {
    pTable = myPieces;
  } else {
    while(pPtr->next)
      pPtr = pPtr->next;
    pPtr->next = myPieces;
  }
  return;
}

pEntry* getMyPieces(char* name) {
  pEntry* pPtr = pTable;
  while(pPtr) {
    if(strcmp(pPtr->name, name) == 0)
      break;
    pPtr = pPtr->next;
  }
  return pPtr;
}

int rmMyPieces(pEntry* myPieces) {
  pEntry* pPtr = pTable;

  if(pTable == NULL) {
    fprintf(stderr, "--err in file %s func %s: \n--pTable is NULL.\n"
      , __FILE__, __func__);
    return -1;
  }

  if(pPtr == myPieces) {
    pTable = pPtr->next;
  } else {
    while(pPtr->next != myPieces) {
      if((pPtr = pPtr->next) == NULL) {
        fprintf(stderr, "--err in file %s func %s: \n--myPieces not found.\n"
          , __FILE__, __func__);
        return -1;
      }
    }
    pPtr->next = pPtr->next->next;
  }
  free(myPieces);
  myPieces = NULL;
  return 1;
}

int writePieceData(pEntry* myPieces, char* piece, int pNum) {
  int size = (pNum == myPieces->totalPieces - 1) ? 
    myPieces->size % PIECE_SIZE : PIECE_SIZE;
  memcpy(myPieces->data + pNum, piece, size);
  return 1;
}

int startUpThread(pthread_t* tTable, void* arg) {
  int tryNum = 0;
  while (pthread_create((tTable + upthreadCnt), NULL, (void *) p2p_upload, (void *) arg) != 0 
    && tryNum < MAX_THREAD_NUM) {
  fprintf(stderr, "--warning in file %s func %s: \n--creat %dth thread fail.\n"
    , __FILE__, __func__, upthreadCnt);
  tryNum++;
  upthreadCnt = (upthreadCnt + 1) % MAX_THREAD_NUM;
}

if(tryNum < MAX_THREAD_NUM){
    upthreadCnt = (upthreadCnt + 1) % MAX_THREAD_NUM; // move to next slot
    printf("%s:\t\t UPLOADTHREAD AT SLOT[%d] CREATED \n", __func__, upthreadCnt);
    return 1;
  }

  return -1;
}

int startDownThread(pEntry* myPieces, void* arg) {
  P2PInfo* req = (P2PInfo*)arg;
  if (pthread_create((myPieces->threads + req->pNum), NULL, (void *) p2p_download, (void *) arg) != 0) {
  fprintf(stderr, "--warning in file %s func %s: \n--%dth piece create thread fail.\n"
    , __FILE__, __func__, req->pNum);
  return -1;
}
return 1;
}

int connectToRemote(char* ip, int port){
  int sock;
  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == -1) {
    fprintf(stderr, "--err in file %s func %s: \n--could not create sock.\n"
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
    fprintf(stderr, "--err in file %s func %s: \n--listen sock create failed.\n"
      , __FILE__, __func__); 
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
  if (listen(server_socket, LISTEN_BACKLOG)) {
    fprintf(stderr, "--err in file %s func %s: \n--Bind port %d failed.\n"
      , __FILE__, __func__, port); 
    return -1;
  }

  return server_socket;
}

int getPieceFromFile(char* piece, P2PInfo* req) {
  int file = 0;
  int pieceLen, getLen;

  if((file=open(req->name, O_RDONLY)) < 0){
    fprintf(stderr, "--err in file %s func %s: \n--open file %s fails.\n"
      , __FILE__, __func__, req->name); 
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

int getPieceToTransfer(int *pNum, pEntry* myPieces) {
  int idx;
  for(idx = 0; idx < myPieces->totalPieces; idx++) {
    if(myPieces->pieces[idx] == UNTOUCHED) {
      *pNum = idx;
      return 1;
    }
    // although it is downloading, the thread is terminated
    if(myPieces->pieces[idx] == DOWNLOADING 
      && pthread_timedjoin_np(myPieces->threads[idx], NULL) == 0) {
      *pNum = idx;
    return 1;
    }
  }
  return -1;
}

int initNewReq(P2PInfo* newReq, P2PInfo* req, int pNum, char* peerIP, char* myIP) {
  newReq = (P2PInfo*)malloc(sizeof(P2PInfo));
  newReq->type = REQ;
  memcpy(newReq->name, req->name, FILE_NAME_LEN);
  newReq->size = req->size;
  newReq->pNum = pNum;
  newReq->srcPort = 0; // will be initiated later
  newReq->destPort = P2P_LISTENING_PORT; 
  strcpy(newReq->srcIP, myIP);
  strcpy(newReq->destIP, peerIP);
  if(newReq->srcIP == NULL)
    return -1;
  else
    return 1;
}

// reference: http://stackoverflow.com/questions/2283494/get-ip-address-of-an-interface-on-linux
// reference: http://man7.org/linux/man-pages/man3/getifaddrs.3.html
int getMyIp(char* host) {
  // int fd;
  // struct ifreq ifr;

  // fd = socket(AF_INET, SOCK_DGRAM, 0);

  // /* I want to get an IPv4 IP address */
  // ifr.ifr_addr.sa_family = AF_INET;

  //  I want IP address attached to "eth0" 
  // strncpy(ifr.ifr_name, "eth0", IFNAMSIZ-1);

  // ioctl(fd, SIOCGIFADDR, &ifr);

  // close(fd);

  // /* display result */
  // return inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
  struct ifaddrs *ifaddr, *ifa;
  int family, s, n;

  if (getifaddrs(&ifaddr) == -1) {
   perror("getifaddrs");
   exit(EXIT_FAILURE);
 }

  /* Walk through linked list, maintaining head pointer so we
    can free list later */

 for (ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++) {
   if (ifa->ifa_addr == NULL)
     continue;

   family = ifa->ifa_addr->sa_family;

     /* Display interface name and family (including symbolic
        form of the latter for the common families) */

     // printf("%-8s %s (%d)\n",
     //        ifa->ifa_name,
     //        (family == AF_PACKET) ? "AF_PACKET" :
     //        (family == AF_INET) ? "AF_INET" :
     //        (family == AF_INET6) ? "AF_INET6" : "???",
     //        family);

     /* For an AF_INET* interface address, display the address */

   if (family == AF_INET || family == AF_INET6) {
     s = getnameinfo(ifa->ifa_addr,
       (family == AF_INET) ? sizeof(struct sockaddr_in) :
       sizeof(struct sockaddr_in6),
       host, NI_MAXHOST,
       NULL, 0, NI_NUMERICHOST);
     if (s != 0) {
       printf("getnameinfo() failed: %s\n", gai_strerror(s));
       return 0;
     }
     if(strcmp("127.0.0.1", host) != 0 && strlen(host) <= 16){
       freeifaddrs(ifaddr);
       return 1;
     }

         // printf("\t\taddress: <%s>\n", host);

   } else if (family == AF_PACKET && ifa->ifa_data != NULL) {
     struct rtnl_link_stats *stats = ifa->ifa_data;

         // printf("\t\ttx_packets = %10u; rx_packets = %10u\n"
         //        "\t\ttx_bytes   = %10u; rx_bytes   = %10u\n",
         //        stats->tx_packets, stats->rx_packets,
         //        stats->tx_bytes, stats->rx_bytes);
   }
 }
 freeifaddrs(ifaddr);
 return -1;
}

int check_P2PMsg(char* buff) {
  P2PInfo* req = (P2PInfo*)buff;
  if(req->type != REQ 
    && req->type != REQFIN 
    && req->type != REREQ 
    )
    return -1;
  else
    return 1;
}

int getPeerIPFromPT(char* peerIP, char* filename) {
  return 1;
}
void peer_table_add(char* ip, char* name, unsigned long timestamp, int downSock, int reqSock) {

}
void peer_table_rm(char* ip, char* name, unsigned long timestamp, int downSock, int reqSock) {
  
}