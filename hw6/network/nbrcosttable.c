
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "nbrcosttable.h"
#include "../common/constants.h"
#include "../topology/topology.h"

//This function creates a neighbor cost table dynamically 
//and initialize the table with all its neighbors' node IDs and direct link costs.
//The neighbors' node IDs and direct link costs are retrieved from topology.dat file. 
nbr_cost_entry_t* nbrcosttable_create()
{
  int nbrNum = topology_getNbrNum(), i, myNodeId = topology_getMyNodeID();
  int* nodeIdArray = topology_getNbrArray();
  
  nbr_cost_entry_t* nct = (nbr_cost_entry_t*)malloc(nbrNum * sizeof(nbr_cost_entry_t));
  if(nodeIdArray == NULL) {
   fprintf(stderr, "err in file %s func %s line %d: topology_getNodeArray err.\n"
    , __FILE__, __func__, __LINE__); 
    return NULL;
  }
  
  for(i = 0; i < nbrNum; i++) {
    nct[i].nodeID = nodeIdArray[i];
    nct[i].cost = topology_getCost(myNodeId, nodeIdArray[i]); 
  }
  
  free(nodeIdArray);
  return nct;
}

//This function destroys a neighbor cost table. 
//It frees all the dynamically allocated memory for the neighbor cost table.
void nbrcosttable_destroy(nbr_cost_entry_t* nct)
{
  if(nct != NULL) {
    free(nct);
    nct = NULL;
    return;
  }
  fprintf(stderr, "err in file %s func %s line %d: nt is null.\n"
    , __FILE__, __func__, __LINE__); 
}

//This function is used to get the direct link cost from neighbor.
//The direct link cost is returned if the neighbor is found in the table.
//INFINITE_COST is returned if the node is not found in the table.
unsigned int nbrcosttable_getcost(nbr_cost_entry_t* nct, int nodeID)
{
  return 0;
}

//This function prints out the contents of a neighbor cost table.
void nbrcosttable_print(nbr_cost_entry_t* nct)
{
  if(nct != NULL) {
    int nbrNum = topology_getNbrNum(), i, myNodeId = topology_getMyNodeID();
    printf("%s:\n", __func__);
    for(i = 0; i < nbrNum; i++) {
      printf("--|%d -> %d : %d\n", myNodeId, nct[i].nodeID, nct[i].cost);
    }
    return;
  }
  fprintf(stderr, "err in file %s func %s line %d: nt is null.\n"
    , __FILE__, __func__, __LINE__); 
  return;
}
