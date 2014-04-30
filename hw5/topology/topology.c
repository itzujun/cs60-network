//FILE: topology/topology.c
//
//Description: this file implements some helper functions used to parse 
//the topology file 
//
//Date: May 3,2010

#include "topology.h"
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/unistd.h>
#include <strings.h>

//this function returns node ID of the given hostname
//the node ID is an integer of the last 8 digit of the node's IP address
//for example, a node with IP address 202.120.92.3 will have node ID 3
//if the node ID can't be retrieved, return -1
int topology_getNodeIDfromname(char* hostname) 
{
  struct hostent* host = NULL;
  int nodeId;
  if((host = gethostbyname(hostname)) == NULL) {
    fprintf(stderr, "err in file %s func %s line %d: gethostbyname err.\n"
      , __FILE__, __func__, __LINE__); 
    return -1;
  }
  nodeId = topology_getNodeIDfromip((struct in_addr*)host->h_addr_list[0]);
  if(nodeId == -1) {
    fprintf(stderr, "err in file %s func %s line %d: inet_ntoa err.\n"
      , __FILE__, __func__, __LINE__); 
    return -1;
  }
  //printf("%s: nodeid is %d\n", __func__, nodeId);
  return nodeId;
}

//this function returns node ID from the given IP address
//if the node ID can't be retrieved, return -1
int topology_getNodeIDfromip(struct in_addr* addr)
{
  struct sockaddr_in sa;
  char inetadd[INET_ADDRSTRLEN]; 
  char *nodeId;
  inet_ntop(AF_INET, addr, inetadd, INET_ADDRSTRLEN);
  if(inetadd == NULL) {
    fprintf(stderr, "err in file %s func %s line %d: inet_ntoa err.\n",
     __FILE__, __func__, __LINE__); 
    return -1;
  }
  nodeId = rindex(inetadd, '.');
  if(nodeId == NULL) {
    fprintf(stderr, "err in file %s func %s line %d: rindex err.\n",
     __FILE__, __func__, __LINE__); 
    return -1;
  }
  // printf("%s: ip is %s, nodeid is %d\n", __func__, inetadd, atoi(nodeId + 1));
  return atoi(nodeId + 1);
}

//this function returns my node ID
//if my node ID can't be retrieved, return -1
int topology_getMyNodeID()
{
  char hostname[1024];
  int nodeId;
  if(gethostname(hostname, 1024) == -1) {
    fprintf(stderr, "err in file %s func %s line %d: gethostname err.\n",
     __FILE__, __func__, __LINE__); 
    return -1;
  }  
  nodeId = topology_getNodeIDfromname(hostname);
  if(nodeId == -1) {
    fprintf(stderr, "err in file %s func %s line %d: topology_getNodeIDfromname err.\n",
     __FILE__, __func__, __LINE__); 
    return -1;
  }  
  //printf("%s: nodeid is %d\n", __func__, nodeId);
  return nodeId;
}

//this functions parses the topology information stored in topology.dat
//returns the number of neighbors
int topology_getNbrNum()
{
  // get hostname
  char hostname[1024];
  if(gethostname(hostname, 1024) == -1) {
    fprintf(stderr, "err in file %s func %s line %d: gethostname err.\n",
     __FILE__, __func__, __LINE__); 
    return -1;
  }  

  return topology_getNbrNumByName(hostname);
}

int topology_getNbrNumByName(char* hostname) {
  FILE *pFile;
  size_t len;
  char hname1[128], hname2[128];
  int cost, nbrNum = 0;

  pFile = fopen("topology/topology.dat", "r");
  if(pFile == NULL) {
    fprintf(stderr, "err in file %s func %s line %d: fopen err.\n",
     __FILE__, __func__, __LINE__); 
    return -1;
  }
  
  while (!feof(pFile)) {
    fscanf(pFile, "%s %s %d", hname1, hname2, &cost);
    //printf("%s: %s %s %d, and hostname %s\n", __func__, hname1, hname2, cost, hostname);
    if(strcmp(hname1, hostname) * strcmp(hname2, hostname) == 0) {
      nbrNum++;
      //printf("%s: nbrNum is %d\n", __func__, nbrNum);
    }
  }
  fclose(pFile);
  return nbrNum - 1;
}

//this functions parses the topology information stored in topology.dat
//returns the number of total nodes in the overlay 
int topology_getNodeNum()
{ 
  FILE *pFile;
  size_t len;
  char hname1[128], hname2[128];
  int cost, nodeNum = 0, nodeId;
  int idSet[MAX_NODE_NUM];
  bzero(idSet, MAX_NODE_NUM * sizeof(int));

  pFile = fopen("topology/topology.dat", "r");
  if(pFile == NULL) {
    fprintf(stderr, "err in file %s func %s line %d: fopen err.\n",
     __FILE__, __func__, __LINE__); 
    return -1;
  }
  while (!feof(pFile)) {
    fscanf(pFile, "%s %s %d", hname1, hname2, &cost);
    nodeId = topology_getNodeIDfromname(hname1);
    if (idSet[nodeId] == 0) {
      idSet[nodeId] = 1;
      nodeNum++;
    }
    nodeId = topology_getNodeIDfromname(hname2);
    if (idSet[nodeId] == 0) {
      idSet[nodeId] = 1;
      nodeNum++;
    }
  }
  fclose(pFile);
  return nodeNum - 1;  
}

//this functions parses the topology information stored in topology.dat
//returns a dynamically allocated array which contains all the nodes' IDs in the overlay network  
int* topology_getNodeArray()
{
  FILE *pFile;
  size_t len;
  char hname1[128], hname2[128], hostname[1024];
  int cost, nodeNum = 0, nodeId, i, idx = 0;
  int idSet[MAX_NODE_NUM];
  int* nodeArray;
  bzero(idSet, MAX_NODE_NUM * sizeof(int));

  if(gethostname(hostname, 1024) == -1) {
    fprintf(stderr, "err in file %s func %s line %d: gethostname err.\n",
     __FILE__, __func__, __LINE__); 
    return NULL;
  }  

  // build id set and count
  pFile = fopen("topology/topology.dat", "r");
  if(pFile == NULL) {
    fprintf(stderr, "err in file %s func %s line %d: fopen err.\n",
     __FILE__, __func__, __LINE__); 
    return NULL;
  }
  while (!feof(pFile)) {
    fscanf(pFile, "%s %s %d", hname1, hname2, &cost);
    //printf("%s: %s %s %d, and hostname %s\n", __func__, hname1, hname2, cost, hostname);
    nodeId = topology_getNodeIDfromname(hname1);
    //printf("%s: nodeId %d\n", __func__, nodeId);
    if (idSet[nodeId] == 0) {
      idSet[nodeId] = 1;
      nodeNum++;
    }
    nodeId = topology_getNodeIDfromname(hname2);
    //printf("%s: nodeId %d\n", __func__, nodeId);
    if (idSet[nodeId] == 0) {
      idSet[nodeId] = 1;
      nodeNum++;
    }
  }
  fclose(pFile);
  if(nodeId = topology_getNodeIDfromname(hostname) == -1) {
    fprintf(stderr, "err in file %s func %s line %d: topology_getNodeIDfromname err.\n",
     __FILE__, __func__, __LINE__); 
    return NULL;
  }  

  // build node array based on id set
  if(idSet == NULL) {
    fprintf(stderr, "err in file %s func %s line %d: idSet is null.\n",
     __FILE__, __func__, __LINE__); 
    return NULL;
  }
  if(nodeNum == 0) {
    fprintf(stderr, "err in file %s func %s line %d: length is 0.\n",
     __FILE__, __func__, __LINE__); 
    return NULL;
  }
  return idxArray2NodeArray(idSet, nodeNum);  
}

//this functions parses the topology information stored in topology.dat
//returns a dynamically allocated array which contains all the neighbors'IDs  
int* topology_getNbrArray()
{
  // get hostname
  char hostname[1024];
  if(gethostname(hostname, 1024) == -1) {
    fprintf(stderr, "err in file %s func %s line %d: gethostname err.\n",
     __FILE__, __func__, __LINE__); 
    return 0;
  } 

  return topology_getNbrArrayByName(hostname);
}

int* topology_getNbrArrayByName(char* hostname) {
  FILE *pFile;
  size_t len;
  char hname1[128], hname2[128];
  int cost, nodeNum = 0, nodeId, i, idx = 0;
  int idSet[MAX_NODE_NUM];
  bzero(idSet, MAX_NODE_NUM * sizeof(int));

  // build nbr idx arrey and count 
  pFile = fopen("topology/topology.dat", "r");
  if(pFile == NULL) {
    fprintf(stderr, "err in file %s func %s line %d: fopen err.\n",
     __FILE__, __func__, __LINE__); 
    return;
  }
  while (!feof(pFile)) {
    fscanf(pFile, "%s %s %d", hname1, hname2, &cost);
    // printf("%s: %s %s %d, and hostname %s\n", __func__, hname1, hname2, cost, hostname);
    if (strcmp(hname1, hostname) == 0) {
      nodeId = topology_getNodeIDfromname(hname2);
    } else if (strcmp(hname2, hostname) == 0) {
      nodeId = topology_getNodeIDfromname(hname1);
    } else {
      continue;
    }
    if(nodeId == -1) {
      fprintf(stderr, "err in file %s func %s line %d: topology_getNodeIDfromname err.\n",
       __FILE__, __func__, __LINE__); 
      return NULL;
    }
    //printf("%s: nbr nodeId %d\n", __func__, nodeId);
    idSet[nodeId] = 1;
    nodeNum++;
  }
  fclose(pFile);

  // build nbr node array
  if(idSet == NULL) {
    fprintf(stderr, "err in file %s func %s line %d: idSet is null.\n",
     __FILE__, __func__, __LINE__); 
    return NULL;
  }
  if(nodeNum == 0) {
    fprintf(stderr, "err in file %s func %s line %d: length is 0.\n",
     __FILE__, __func__, __LINE__); 
    return NULL;
  }  
  return idxArray2NodeArray(idSet, nodeNum);
}

int* idxArray2NodeArray(int* idSet, int nodeNum) {
  if(idSet == NULL) {
    fprintf(stderr, "err in file %s func %s line %d: idSet is null.\n",
     __FILE__, __func__, __LINE__); 
    return NULL;
  }
  if(nodeNum == 0) {
    fprintf(stderr, "err in file %s func %s line %d: length is 0.\n",
     __FILE__, __func__, __LINE__); 
    return NULL;
  }

  int* nodeArray;
  int i, idx = 0;
  nodeArray = (int*)malloc(nodeNum * sizeof(int));
  for(i = 0; i < MAX_NODE_NUM; i++) {
    //printf("%s: i %d, isset %d\n", __func__, i, idSet[i]);
    if(idSet[i]) {
      //printf("%s: idx %d, nodeid %d\n", __func__, idx, i);
      nodeArray[idx++] = i;
    }
  }  
  return nodeArray;
}

//this functions parses the topology information stored in topology.dat
//returns the cost of the direct link between the two given nodes 
//if no direct link between the two given nodes, INFINITE_COST is returned
unsigned int topology_getCost(int fromNodeID, int toNodeID)
{
  FILE *pFile;
  size_t len;
  char hname1[128], hname2[128];
  int cost, nbrNum = 0;

  pFile = fopen("topology/topology.dat", "r");
  if(pFile == NULL) {
    fprintf(stderr, "err in file %s func %s line %d: fopen err.\n",
     __FILE__, __func__, __LINE__); 
    return 0;
  }
  while (!feof(pFile)) {
    fscanf(pFile, "%s %s %d", hname1, hname2, &cost);
    if(topology_getNodeIDfromname(hname1) == fromNodeID 
      && topology_getNodeIDfromname(hname2) == toNodeID) {
      return cost;
  }
}

return UINT_MAX;
}

char* getHostnameFromNodeId(int nid) {
  FILE *pFile;
  size_t len;
  char* hname1 = (char*)malloc(128 * sizeof(char));
  char* hname2 = (char*)malloc(128 * sizeof(char));
  int cost, nodeNum = 0, nodeId;
  int idSet[MAX_NODE_NUM];
  bzero(idSet, MAX_NODE_NUM * sizeof(int));

  pFile = fopen("topology/topology.dat", "r");
  if(pFile == NULL) {
    fprintf(stderr, "err in file %s func %s line %d: fopen err.\n",
     __FILE__, __func__, __LINE__); 
    return NULL;
  }
  while (!feof(pFile)) {
    fscanf(pFile, "%s %s %d", hname1, hname2, &cost);
    nodeId = topology_getNodeIDfromname(hname1);
    if (idSet[nodeId] == 0) {
      idSet[nodeId] = 1;
      if(nodeId == nid)
        return hname1;
    }
    nodeId = topology_getNodeIDfromname(hname2);
    if (idSet[nodeId] == 0) {
      idSet[nodeId] = 1;
      if(nodeId == nid)
        return hname2;
    }
  }
  fclose(pFile);
  return NULL;
}

struct in_addr getIpFromNodeId(int nid) {
  struct hostent* host =NULL;
  struct in_addr* ip;
  char* hostname;

  
  if((hostname = getHostnameFromNodeId(nid)) == NULL) {
    printf("%s: nid is %d\n", __func__, nid);
    fprintf(stderr, "err in file %s func %s line %d: getHostnameFromNodeId err.\n",
      __FILE__, __func__, __LINE__); 
  }
  if((host = gethostbyname(hostname)) == NULL) {
    fprintf(stderr, "err in file %s func %s line %d: gethostbyname err.\n",
      __FILE__, __func__, __LINE__); 
  }  
  return *((struct in_addr*)host->h_addr_list[0]);
}
