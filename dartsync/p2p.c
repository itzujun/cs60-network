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

/**
steps:
  create server side sock;
  listening and binding;
  run the accept function that watis for p2p_download thread;
  create p2p_upload thread with coressponding file and peiceSeqNum;
*/
void* p2p_listening(void* arg) {


}

/**
steps:
  connect to p2p_upload using specific port;
  send the piece request and the randomly generated port;
  close connection;
  create a server sock and start listening;
  recv data from p2p_upload;
  if recv is finished, send fin signal to remote p2p_upload;
  close connection;
  send handshake to server use the file_monitor module built in function;
*/
void* p2p_download(void* arg) {

}

/**
steps:
  try to connect to remote p2p_download thread;
  wait for file fin signal;
*/
void* p2p_upload(void* arg) {

}
