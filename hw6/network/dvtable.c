
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../common/constants.h"
#include "../topology/topology.h"
#include "dvtable.h"

//This function creates a dvTable(distance vector table) dynamically.
//A distance vector table contains the n+1 entries, where n is the number of the neighbors of this node, and the rest one is for this node itself. 
//Each entry in distance vector table is a dv_t structure which contains a source node ID and an array of N dv_entry_t structures where N is the number of all the nodes in the overlay.
//Each dv_entry_t contains a destination node address the the cost from the source node to this destination node.
//The dvTable is initialized in this function.
//The link costs from this node to its neighbors are initialized using direct link cost retrived from topology.dat. 
//Other link costs are initialized to INFINITE_COST.
//The dynamically created dvTable is returned.
dv_t* dvtable_create()
{
  // initiate dvTable memory
  int i, j, nbrNum = topology_getNbrNum(), nodeNum = topology_getNodeNum();
  int myNodeId = topology_getMyNodeID();
  int* nodeIdArray = topology_getNodeArray();
  int* nbrIdArray = topology_getNbrArrayAndSelf();
  dv_t* dvTable = (dv_t*)malloc((nbrNum + 1) * sizeof(dv_t));
  
  //printf("%s: nodeNum is %d\n", __func__, nodeNum);
  for (i = 0; i < nbrNum + 1; i++) {
    dvTable[i].dvEntry = (dv_entry_t*)malloc(nodeNum * sizeof(dv_entry_t));
    dvTable[i].nodeID = nbrIdArray[i];
    //printf("%s: nodeID of this dvEntry is %d\n", __func__, dvTable[i].nodeID);
    for (j = 0; j < nodeNum; j++) {
      dvTable[i].dvEntry[j].nodeID = nodeIdArray[j];
      if(myNodeId == dvTable[i].nodeID) // if self, then add nbr cost {
        dvTable[i].dvEntry[j].cost = topology_getCost(dvTable[i].nodeID, nodeIdArray[j]);
      else  // if not self entry, then set infinite
        dvTable[i].dvEntry[j].cost = INFINITE_COST;
    }
  }
  return dvTable;
}

//This function destroys a dvTable. 
//It frees all the dynamically allocated memory for the dvTable.
void dvtable_destroy(dv_t* dvTable)
{
  if(dvTable != NULL) {
    int i, nbrNum = topology_getNbrNum();
    for(i = 0; i < nbrNum + 1; i++) {
      if(dvTable[i].dvEntry) {
        free(dvTable[i].dvEntry);
        dvTable[i].dvEntry = NULL;
      } else {
        fprintf(stderr, "err in file %s func %s line %d: dvTable[i].dvEntry is null.\n"
          , __FILE__, __func__, __LINE__); 
      }
    }
    free(dvTable);
    dvTable = NULL;
    return;
  }
  fprintf(stderr, "err in file %s func %s line %d: dvTable is null.\n"
    , __FILE__, __func__, __LINE__); 
  return;
}

//This function sets the link cost between two nodes in dvTable.
//If those two nodes are found in the table and the link cost is set, return 1.
//Otherwise, return -1.
int dvtable_setcost(dv_t* dvTable,int fromNodeID,int toNodeID, unsigned int cost)
{
  int i, j, nbrNum = topology_getNbrNum(), nodeNum = topology_getNodeNum();
  for (i = 0; i < nbrNum + 1; i++) {
    if(dvTable[i].nodeID == fromNodeID) {
      for (j = 0; j < nodeNum; j++) {
        if(dvTable[i].dvEntry[j].nodeID == toNodeID) {
          dvTable[i].dvEntry[j].cost = cost;
          return 1;
        }
      }
    }
  }
  fprintf(stderr, "err in file %s func %s line %d: node pair %d -> %d not found.\n"
    , __FILE__, __func__, __LINE__, fromNodeID, toNodeID); 
  return -1;
}

//This function returns the link cost between two nodes in dvTable
//If those two nodes are found in dvTable, return the link cost. 
//otherwise, return INFINITE_COST.
unsigned int dvtable_getcost(dv_t* dvTable, int fromNodeID, int toNodeID)
{
  int i, j, nbrNum = topology_getNbrNum(), nodeNum = topology_getNodeNum();
  for (i = 0; i < nbrNum + 1; i++) {
    if(dvTable[i].nodeID == fromNodeID) {
      for (j = 0; j < nodeNum; j++) {
        if(dvTable[i].dvEntry[j].nodeID == toNodeID) {
          return dvTable[i].dvEntry[j].cost;
        }
      }
    }
  }
  return INFINITE_COST;
}

//This function prints out the contents of a dvTable.
void dvtable_print(dv_t* dvTable)
{
  if(dvTable != NULL) {
    int i, j, nbrNum = topology_getNbrNum(), nodeNum = topology_getNodeNum();
    printf("%s:\n", __func__);
    for (i = 0; i < nbrNum + 1; i++) {
      printf("--|%d -> {", dvTable[i].nodeID);
      for (j = 0; j < nodeNum; j++) {
        printf("%d : %d, ", dvTable[i].dvEntry[j].nodeID, dvTable[i].dvEntry[j].cost);
      }
      printf("}\n");
    }
    return;
  }
  fprintf(stderr, "err in file %s func %s line %d: dvTable is null.\n"
    , __FILE__, __func__, __LINE__); 
  return;
}
