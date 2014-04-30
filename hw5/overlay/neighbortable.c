//FILE: overlay/neighbortable.c
//
//Description: this file the API for the neighbor table
//
//Date: May 03, 2010

#include "neighbortable.h"
#include "../topology/topology.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <sys/utsname.h>
#include <assert.h>

//This function first creates a neighbor table dynamically. It then parses the topology/topology.dat file and fill the nodeID and nodeIP fields in all the entries, initialize conn field as -1 .
//return the created neighbor table
nbr_entry_t* nt_create()
{
  int nbrNum = topology_getNbrNum(), i;
  nbr_entry_t* nt = (nbr_entry_t*)malloc(nbrNum * sizeof(nbr_entry_t));
  int* nodeIdArray = topology_getNodeArray();
  if(nodeIdArray == NULL) {
   fprintf(stderr, "err in file %s func %s line %d: topology_getNodeArray err.\n"
    , __FILE__, __func__, __LINE__); 
    return NULL;
  }

  for(i = 0; i < nbrNum; i++) {
    nt[i].nodeID = nodeIdArray[i];
    struct in_addr nip = getIpFromNodeId(nodeIdArray[i]);
    memcpy(&nt[i].nodeIP, &nip, sizeof(in_addr_t));
    nt[i].conn = -1;
  }
  free(nodeIdArray);
  return nt;
}

//This function destroys a neighbortable. It closes all the connections and frees all the dynamically allocated memory.
void nt_destroy(nbr_entry_t* nt)
{
  int i, nbrNum = topology_getNbrNum();
  if(nt != NULL) {
    free(nt);
    nt = NULL;
    return;
  }
  fprintf(stderr, "err in file %s func %s line %d: nt is null.\n"
    , __FILE__, __func__, __LINE__); 
}

//This function is used to assign a TCP connection to a neighbor table entry for a neighboring node. If the TCP connection is successfully assigned, return 1, otherwise return -1
int nt_addconn(nbr_entry_t* nt, int nodeID, int conn)
{
  int i, nbrNum = topology_getNbrNum();
  for(i = 0; i < nbrNum; i++) {
    if(nt[i].nodeID == nodeID) { 
      nt[i].conn = conn;
      return 1;
    }
  }
  return -1;
}