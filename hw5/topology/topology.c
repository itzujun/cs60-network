//FILE: topology/topology.c
//
//Description: this file implements some helper functions used to parse 
//the topology file 
//
//Date: May 3,2010

#include "topology.h"

//this function returns node ID of the given hostname
//the node ID is an integer of the last 8 digit of the node's IP address
//for example, a node with IP address 202.120.92.3 will have node ID 3
//if the node ID can't be retrieved, return -1
int topology_getNodeIDfromname(char* hostname) 
{
  struct hostent* host =NULL;
  int nodeId;
  if((host = gethostbyname(hostname)) == NULL) {
    if(inetadd = inet_ntoa(addr) == NULL) {
      fprintf(stderr, "err in file %s func %s line %d: gethostbyname err.\n"
        , __FILE__, __func__, __LINE__); 
      return -1;
    }
  }
  if((nodeId = topology_getNodeIDfromip((struct in_addr*)h->h_addr_list[0])) == -1) {
   fprintf(stderr, "err in file %s func %s line %d: inet_ntoa err.\n"
    , __FILE__, __func__, __LINE__); 
   return -1;
 }
 return nodeId;
}

//this function returns node ID from the given IP address
//if the node ID can't be retrieved, return -1
int topology_getNodeIDfromip(struct in_addr* addr)
{
  char *inetadd, *nodeId;
  if(inetadd = inet_ntoa(addr) == NULL) {
    fprintf(stderr, "err in file %s func %s line %d: inet_ntoa err.\n",
     __FILE__, __func__, __LINE__); 
    return -1;
  }
  if(nodeId = rindex(inetadd, '.') == NULL) {
    fprintf(stderr, "err in file %s func %s line %d: rindex err.\n",
     __FILE__, __func__, __LINE__); 
    return -1;
  }
  return atoi(p + 1);
}

//this function returns my node ID
//if my node ID can't be retrieved, return -1
int topology_getMyNodeID()
{
  char hostname[1024];
  gethostname(hostname, 1024);
  int nodeId;
  if(hostname == -1) {
    fprintf(stderr, "err in file %s func %s line %d: gethostname err.\n",
     __FILE__, __func__, __LINE__); 
    return -1;
  }  
  if(nodeId = topology_getNodeIDfromname(hostname) == -1) {
    fprintf(stderr, "err in file %s func %s line %d: topology_getNodeIDfromname err.\n",
     __FILE__, __func__, __LINE__); 
    return -1;
  }  
  return nodeId;
}

//this functions parses the topology information stored in topology.dat
//returns the number of neighbors
int topology_getNbrNum()
{
  // get hostname
  char hostname[1024];
  gethostname(hostname, 1024);
  if(hostname == -1) {
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

  if(pFile = fopen("topology.dat", "r") == NULL) {
    fprintf(stderr, "err in file %s func %s line %d: fopen err.\n",
     __FILE__, __func__, __LINE__); 
    return -1;
  }
  while (!feof(pFile)) {
    fscanf(pFile, "%s %s %d", hname1, hname2, &cost);
    if(strcmp(hname1, hostname) * strcmp(hname2, hostname) == 0) {
      nbrNum++;
    }
  }
  fclose(pFile);
  return nbrNum;
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

  if(pFile = fopen("topology.dat", "r") == NULL) {
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
  return nodeNum;  
}

//this functions parses the topology information stored in topology.dat
//returns a dynamically allocated array which contains all the nodes' IDs in the overlay network  
int* topology_getNodeArray()
{
  FILE *pFile;
  size_t len;
  char hname1[128], hname2[128];
  int cost, nodeNum = 0, nodeId, i, idx = 0;
  int idSet[MAX_NODE_NUM];
  int* nodeArray;
  bzero(idSet, MAX_NODE_NUM * sizeof(int));

  // build id set and count
  if(pFile = fopen("topology.dat", "r") == NULL) {
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
  if(nodeId = topology_getNodeIDfromname(hostname) == -1) {
    fprintf(stderr, "err in file %s func %s line %d: topology_getNodeIDfromname err.\n",
     __FILE__, __func__, __LINE__); 
    return -1;
  }  

  // build node array based on id set
  if(idSet == NULL) {
    fprintf(stderr, "err in file %s func %s line %d: idSet is null.\n",
     __FILE__, __func__, __LINE__); 
    return -1;
  }
  if(nodeNum == 0) {
    fprintf(stderr, "err in file %s func %s line %d: length is 0.\n",
     __FILE__, __func__, __LINE__); 
    return -1;
  }
  return idxArray2NodeArray(idSet, nodeNum);  
}

//this functions parses the topology information stored in topology.dat
//returns a dynamically allocated array which contains all the neighbors'IDs  
int* topology_getNbrArray()
{
  // get hostname
  char hostname[1024];
  gethostname(hostname, 1024);
  if(hostname == -1) {
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
  if(pFile = fopen("topology.dat", "r") == NULL) {
    fprintf(stderr, "err in file %s func %s line %d: fopen err.\n",
     __FILE__, __func__, __LINE__); 
    return -1;
  }
  while (!feof(pFile)) {
    fscanf(pFile, "%s %s %d", hname1, hname2, &cost);
    if (strcmp(hname1, hostname) == 0) {
      nodeId = topology_getNodeIDfromname(hname1);
    } else if (strcmp(hname2, hostname) == 0) {
      nodeId = topology_getNodeIDfromname(hname1);
    } else {
      continue;
    }
    if(nodeId == -1) {
      fprintf(stderr, "err in file %s func %s line %d: topology_getNodeIDfromname err.\n",
       __FILE__, __func__, __LINE__); 
      return -1;
    }
    idSet[nodeId] = 1;
    nodeNum++;
  }
  fclose(pFile);

  // build nbr node array
  if(idSet == NULL) {
    fprintf(stderr, "err in file %s func %s line %d: idSet is null.\n",
     __FILE__, __func__, __LINE__); 
    return -1;
  }
  if(nodeNum == 0) {
    fprintf(stderr, "err in file %s func %s line %d: length is 0.\n",
     __FILE__, __func__, __LINE__); 
    return -1;
  }  
  return idxArray2NodeArray(idSet, nodeNum);
}

int* idxArray2NodeArray(int* idSet, int nodeNum) {
  if(idSet == NULL) {
    fprintf(stderr, "err in file %s func %s line %d: idSet is null.\n",
     __FILE__, __func__, __LINE__); 
    return -1;
  }
  if(nodeNum == 0) {
    fprintf(stderr, "err in file %s func %s line %d: length is 0.\n",
     __FILE__, __func__, __LINE__); 
    return -1;
  }

  int* nodeArray;
  nodeArray = (int*)malloc(nodeNum * sizeof(int));
  for(i = 0; i < MAX_NODE_NUM; i++) {
    if(idSet[i]) {
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

  if(pFile = fopen("topology.dat", "r") == NULL) {
    fprintf(stderr, "err in file %s func %s line %d: fopen err.\n",
     __FILE__, __func__, __LINE__); 
    return -1;
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
