#ifndef FILETABLE_H
#define FILETABLE_H
#include "constants.h"
#include <pthread.h>

#define DONE 0 
#define UPDATING 1

//each file can be represented as a node in file table
typedef struct node{
	char name[FILE_NAME_MAX_LEN]; 
	int size;
	char md5[MD5_LEN]; 
	int peer_ip_num;
	int status;
	char newpeerip[MAX_PEER_NUM][IP_LEN];
	struct node *pNext;
}Node;


Node * filetable_head;
Node * filetable_tail;
pthread_mutex_t * file_table_mutex; // mutex for file table
//init table
void initFileTable();
//get filetable size
int getFileTableSize();
//get file by name
Node * findFileEntryByName(char * name);
//append ll
void appendFileEntry(Node * newEntry);
void appendFileEntryNoLock(Node * newEntry);
void updateFileEntry(Node* oldEntry, Node * newEntry);
Node* deleteFileEntry(char* name);
void printFileTable();
int getIPFromFfiletable(char* peerIP, char* filename,Node*fte);
#endif
