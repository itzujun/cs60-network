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
	printf("updateFileEntry %s\n",newEntry->name);
	strcpy(oldEntry->name, newEntry->name);
	oldEntry->size = newEntry->size;
	strcpy(oldEntry->md5, newEntry->md5);
	oldEntry->peer_ip_num = newEntry->peer_ip_num;
	memcpy(oldEntry->newpeerip,newEntry->newpeerip, MAX_PEER_NUM * IP_LEN);
 	pthread_mutex_unlock(file_table_mutex);
	printFileTable();
}

void appendFileEntry(Node * newEntry){
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
	Node * pre = NULL,*cur = NULL;
	while(itr != NULL){
		if(strcmp(name, itr -> name) == 0){
		        if(itr == filetable_head){
				cur = itr;
				filetable_head = itr->pNext;
				if(filetable_head == NULL) 
					filetable_tail = NULL;
			}else{
				cur = itr;
				pre->pNext = itr->pNext;
				if(itr == filetable_tail)
					filetable_tail = pre;
			}	
			free(cur);
			break;
		}
		pre = itr;
		itr = itr -> pNext;
	}
 	pthread_mutex_unlock(file_table_mutex);
	printFileTable();
	if (pre == NULL) return NULL;
	else return pre->pNext;
	
}

void printFileTable(){
	printf("===========file table===========\n");
 	pthread_mutex_lock(file_table_mutex);
	Node * itr = filetable_head;
	while(itr != NULL){
		printf(" name: %s\tsize: %8d\tmd5: %s\tpeer_ip_num: %d\t", itr->name, itr ->size,itr->md5,itr->peer_ip_num);
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

