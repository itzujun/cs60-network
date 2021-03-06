#include "filetable.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/time.h>
#include "peertable.h"

void initFileTable(){
	filetable_head = NULL;
	filetable_tail = NULL;
	file_table_mutex = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(file_table_mutex, NULL);
}

int getFileTableSize(){
	int i = 0;
	Node * itr = filetable_head;
	if(itr == NULL){
		return i;
	}
	while(itr != NULL){
		i++;
		itr = itr -> pNext;
	}
	return i;
}


Node* findFileEntryByName(char * name){
 	pthread_mutex_lock(file_table_mutex);
	Node * itr = filetable_head;
	while(itr){
		if(strcmp(name, itr -> name) == 0){
 			pthread_mutex_unlock(file_table_mutex);
			return itr;
		}
		itr = itr->pNext;
	}
 	pthread_mutex_unlock(file_table_mutex);
	return itr;
}


void updateFileEntry(Node* oldEntry, Node * newEntry){
 	pthread_mutex_lock(file_table_mutex);
 	//memcpy(oldEntry, newEntry, sizeof(Node));
	printf("updateFileEntry %s\n", newEntry->name);
	strcpy(oldEntry->name, newEntry->name);
	oldEntry->size = newEntry->size;
	strcpy(oldEntry->md5, newEntry->md5);
	oldEntry->peer_ip_num = newEntry->peer_ip_num;
	if(oldEntry -> status != UPDATING) {
	  oldEntry -> status = newEntry -> status;
	  oldEntry -> action_time = newEntry -> action_time;
	}
	memcpy(oldEntry->newpeerip,newEntry->newpeerip, MAX_PEER_NUM * IP_LEN);
 	pthread_mutex_unlock(file_table_mutex);
	printFileTable();
}

void appendFileEntryNoLock(Node * newEntry){
  const char* ext = get_filename_ext(newEntry->name);
  if(strcmp(ext, "tmp") == 0 || strcmp(ext, "p2p") == 0)
    return;
	printf("appendFileEntryNoLock %s\n",newEntry->name);
	if(filetable_tail == NULL){
		//ll is empty
		filetable_head = newEntry;
		filetable_tail = newEntry;
	}else{
		filetable_tail -> pNext = newEntry;
		filetable_tail = newEntry;
	}
	printFileTable();
}
void appendFileEntry(Node * newEntry){
  const char* ext = get_filename_ext(newEntry->name);
  if(strcmp(ext, "tmp") == 0 || strcmp(ext, "p2p") == 0)
    return;

  Node* fte;
  if((fte = findFileEntryByName(newEntry->name)) != NULL){
    fte->status = newEntry->status;
    free(newEntry);
    printFileTable();
    return;  
  }
	printf("appendFileEntry %s\n",newEntry->name);
 	pthread_mutex_lock(file_table_mutex);
	if(filetable_tail == NULL){
		//ll is empty
		filetable_head = newEntry;
		filetable_tail = newEntry;
	}else{
		filetable_tail -> pNext = newEntry;
		filetable_tail = newEntry;
	}
 	pthread_mutex_unlock(file_table_mutex);
	printFileTable();
}


Node* deleteFileEntry(char * name){
	printf("deleteFileEntry %s\n",name);
 	pthread_mutex_lock(file_table_mutex);
	Node * itr = filetable_head;
	if(itr == NULL) return NULL;
	Node * pre = NULL;
	while(itr != NULL){
		//check if target
		if(strcmp(name, itr -> name) == 0){
		        if(itr == filetable_head){
				filetable_head = itr->pNext;
				if(filetable_head == NULL) 
					filetable_tail = NULL;
			}else{
				pre->pNext = itr->pNext;
				if(itr == filetable_tail)
					filetable_tail = pre;
			}	
			free(itr);
			break;
		}
		pre = itr;
		itr = itr -> pNext;
	}
 	pthread_mutex_unlock(file_table_mutex);
	printFileTable();
	if (filetable_head == NULL) return NULL;
	else if (pre == NULL) return filetable_head;
	else return pre->pNext;
}

void printFileTable(){
	printf("===========file table===========\n");
 	pthread_mutex_lock(file_table_mutex);
	Node * itr = filetable_head;
	while(itr != NULL){
		printf(" name: %s\tsize: %8d\tmd5: %s\tpeer_ip_num: %d\t status: %d\t act_time: %lu\t", itr->name, itr ->size,itr->md5,itr->peer_ip_num, itr->status, itr->action_time);
		int i;
		for(i =0; i<itr->peer_ip_num;i++){
			printf("newpeerip[%d]: %s\t",i,itr->newpeerip[i]);
		}
		printf("\n");
		itr = itr -> pNext;
	}
 	pthread_mutex_unlock(file_table_mutex);
	printf("================================\n");
}

// void getFilePeersByFilename(char * filename){
// 	pthread_mutex_lock(file_table_mutex);
// 	Node * file = findFileByName(filename);
// 	if(file == NULL)
// 		return NULL;
// 	else{

// 	}
// 	pthread_mutex_unlock(file_table_mutex);
// }

int getIPFromFfiletable(char* peerIP, char* filename , Node*itr) {
	int i;
	// I copy the following code from findFileEntryByName
	// because of the lock issue
 	pthread_mutex_lock(file_table_mutex);

	for(i = 0; i < itr->peer_ip_num; i++ ) {
		if(  peer_peertable_found(itr->newpeerip[i],filename) < 0) {
			memcpy(peerIP, itr->newpeerip[i], IP_LEN);
 			pthread_mutex_unlock(file_table_mutex);
			return 1;
		}
		//printf("%s:\t\t IP %s FOR FILE %s IS CURRENTLY IN USE\n", __func__, itr->newpeerip[i], filename);
	}
 	pthread_mutex_unlock(file_table_mutex);
	return -1;
}

const char *get_filename_ext(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}



