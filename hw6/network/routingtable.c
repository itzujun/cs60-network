

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../common/constants.h"
#include "../topology/topology.h"
#include "routingtable.h"

//This is the hash function used the by the routing table
//It takes the hash key - destination node ID as input, 
//and returns the hash value - slot number for this destination node ID.
//
//You can copy makehash() implementation below directly to routingtable.c:
//int makehash(int node) {
//	return node%MAX_ROUTINGTABLE_ENTRIES;
//}
//
int makehash(int node)
{
  return node % MAX_ROUTINGTABLE_SLOTS;
}

//This function creates a routing table dynamically.
//All the entries in the table are initialized to NULL pointers.
//Then for all the neighbors with a direct link, create a routing entry using the neighbor itself as the next hop node, and insert this routing entry into the routing table. 
//The dynamically created routing table structure is returned.
routingtable_t* routingtable_create()
{
  int i, nbrNum = topology_getNbrNum(), myNodeId = topology_getMyNodeID();
  int* nbrIdArray = topology_getNbrArrayAndSelf();
  routingtable_t* routingtable = (routingtable_t*)malloc(sizeof(routingtable_t));
  for(i = 0; i < MAX_ROUTINGTABLE_SLOTS; i++) {
    routingtable->hash[i] = NULL;
  }
  
  for(i = 0; i < nbrNum + 1; i++) {
    routingtable_setnextnode(routingtable, nbrIdArray[i], nbrIdArray[i]);
  }
  free(nbrIdArray);
  return routingtable;
}

//This funtion destroys a routing table. 
//All dynamically allocated data structures for this routing table are freed.
void routingtable_destroy(routingtable_t* routingtable)
{
  if(routingtable != NULL) {
    int i;
    for(i = 0; i < MAX_ROUTINGTABLE_SLOTS; i++) {
      if(routingtable->hash[i]) {
        free(routingtable->hash[i]);
        routingtable->hash[i] = NULL;
      }
    }
    free(routingtable);
    routingtable = NULL;
    return;
  }
  fprintf(stderr, "err in file %s func %s line %d: routingtable is null.\n"
    , __FILE__, __func__, __LINE__); 
  return;
}

//This function updates the routing table using the given destination node ID and next hop's node ID.
//If the routing entry for the given destination already exists, update the existing routing entry.
//If the routing entry of the given destination is not there, add one with the given next node ID.
//Each slot in routing table contains a linked list of routing entries due to conflicting hash keys (differnt hash keys (destination node ID here) may have same hash values (slot entry number here)).
//To add an routing entry to the hash table:
//First use the hash function makehash() to get the slot number in which this routing entry should be stored. 
//Then append the routing entry to the linked list in that slot.
void routingtable_setnextnode(routingtable_t* routingtable, int destNodeID, int nextNodeID)
{
  if(routingtable != NULL) {
    routingtable_entry_t* entryPtr = routingtable_getClosestEntry(routingtable, destNodeID);
    if(entryPtr == NULL) {  // not found and the list is empty
      routingtable->hash[makehash(destNodeID)] = (routingtable_entry_t*)malloc(sizeof(routingtable_entry_t));
      //printf("%s: new route on slot [%d]\n", __func__, makehash(destNodeID));
      routingtable->hash[makehash(destNodeID)]->next = NULL;
    } else if(entryPtr->destNodeID != destNodeID) { // not found though the list is not empty
      entryPtr->next = (routingtable_entry_t*)malloc(sizeof(routingtable_entry_t));
      //printf("%s: new overlap route on slot [%d]\n", __func__, makehash(destNodeID));
      entryPtr = entryPtr->next;
      entryPtr->next = NULL;
    }
    // no matter whether we find it or not, we need to upadte the nextNodeID 
    routingtable->hash[makehash(destNodeID)]->destNodeID = destNodeID;
    routingtable->hash[makehash(destNodeID)]->nextNodeID = nextNodeID;
    //printf("%s: mem addr on slot [%d] is %d\n", __func__, makehash(destNodeID), entryPtr);
    return;
  }
  fprintf(stderr, "err in file %s func %s line %d: routingtable is null.\n"
    , __FILE__, __func__, __LINE__); 
  return;
}

// Get routingtable_entry_t
// If exit, return this entry
// If not, return the last entry in the list or NULL (if head is NULL) 
routingtable_entry_t* routingtable_getClosestEntry(routingtable_t* routingtable, int destNodeID) {
  routingtable_entry_t* entryPtr = routingtable->hash[makehash(destNodeID)];
  while(entryPtr != NULL 
    && entryPtr->next != NULL 
    && entryPtr->destNodeID != destNodeID) {
    entryPtr = entryPtr->next;
  }
  return entryPtr;
}

//This function looks up the destNodeID in the routing table.
//Since routing table is a hash table, this opeartion has O(1) time complexity.
//To find a routing entry for a destination node, you should first use the hash function makehash() to get the slot number and then go through the linked list in that slot to search the routing entry.
//If the destNodeID is found, return the nextNodeID for this destination node.
//If the destNodeID is not found, return -1.
int routingtable_getnextnode(routingtable_t* routingtable, int destNodeID)
{
  if(routingtable != NULL) {
    routingtable_entry_t* entryPtr = routingtable_getClosestEntry(routingtable, destNodeID);
    if(entryPtr && entryPtr->destNodeID == destNodeID) {  // not found and the list is empty
      return entryPtr->nextNodeID;
    } else {
      return -1;
    }
  }
  fprintf(stderr, "err in file %s func %s line %d: routingtable is null.\n"
    , __FILE__, __func__, __LINE__); 
  return -1;
}

//This function prints out the contents of the routing table
void routingtable_print(routingtable_t* routingtable)
{
  if(routingtable != NULL) {
    int i, isEmpty = 1;
    printf("%s:\n", __func__);
    for (i = 0; i < MAX_ROUTINGTABLE_SLOTS; i++) {
      routingtable_entry_t* entryPtr = routingtable->hash[i];
      //printf("%s: mem addr on slot [%d] is %d\n", __func__, i, entryPtr);
      while(entryPtr != NULL) {
        printf("--|slot [%d]: dest: %d next: %d\n", i, entryPtr->destNodeID, entryPtr->nextNodeID);
        isEmpty = 0;
        entryPtr = entryPtr->next;
      }
    }
    if(isEmpty)
      printf("%s: routingtable is empty!\n", __func__);
    return;
  }
  fprintf(stderr, "err in file %s func %s line %d: routingtable is null.\n"
    , __FILE__, __func__, __LINE__); 
  return;
}
