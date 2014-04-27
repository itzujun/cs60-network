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
  char *inetadd;
  char *nodeId;
  if(inetadd = inet_ntoa(addr) == NULL) {
    fprintf(stderr, "err in file %s func %s line %d: inet_ntoa err.\n"
      , __FILE__, __func__, __LINE__); 
    return -1;
  }
  if(nodeId = rindex(inetadd, '.') == NULL) {
    fprintf(stderr, "err in file %s func %s line %d: rindex err.\n"
      , __FILE__, __func__, __LINE__); 
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
  if(hostname == -1) {
    fprintf(stderr, "err in file %s func %s line %d: gethostname err.\n"
      , __FILE__, __func__, __LINE__); 
    return -1;
  }  
  return topology_getNodeIDfromname(hostname);
}

//this functions parses the topology information stored in topology.dat
//returns the number of neighbors
int topology_getNbrNum()
{
  // get hostname
  char hostname[1024];
  gethostname(hostname, 1024);
  if(hostname == -1) {
    fprintf(stderr, "err in file %s func %s line %d: gethostname err.\n"
      , __FILE__, __func__, __LINE__); 
    return -1;
  }  

  pch = strtok (str," ,.-");
  while (pch != NULL)
  {
    printf ("%s\n",pch);
    pch = strtok (NULL, " ,.-");
  }
  return 0;
}

int topology_getNbrNumByName(char* hostname) {
  FILE *pFile;
  size_t len;
  char hname1[128], hname2[128];
  int cost, nbrNum = 0;

  if(pFile = fopen("topology.dat", "r") == NULL) {
    fprintf(stderr, "err in file %s func %s line %d: fopen err.\n"
      , __FILE__, __func__, __LINE__); 
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
  int nodeIdSet[256];
  bzero(nodeIdSet, 256 * sizeof(int));

  if(pFile = fopen("topology.dat", "r") == NULL) {
    fprintf(stderr, "err in file %s func %s line %d: fopen err.\n"
      , __FILE__, __func__, __LINE__); 
    return -1;
  }
  while (!feof(pFile)) {
    fscanf(pFile, "%s %s %d", hname1, hname2, &cost);
    nodeId = topology_getNodeIDfromname(hname1);
    if (nodeIdSet[nodeId] == 0) {
      nodeIdSet[nodeId] = 1;
      nodeNum++;
    }
    nodeId = topology_getNodeIDfromname(hname2);
    if (nodeIdSet[nodeId] == 0) {
      nodeIdSet[nodeId] = 1;
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
  return 0;
}

//this functions parses the topology information stored in topology.dat
//returns a dynamically allocated array which contains all the neighbors'IDs  
int* topology_getNbrArray()
{
  int* 
  return 0;
}

//this functions parses the topology information stored in topology.dat
//returns the cost of the direct link between the two given nodes 
//if no direct link between the two given nodes, INFINITE_COST is returned
unsigned int topology_getCost(int fromNodeID, int toNodeID)
{
  return 0;
}
