#include "peertable.h"
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

void initPeerTable(){
	peertable_head = NULL;
	peertable_tail = NULL;
	peer_table_mutex = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(peer_table_mutex, NULL);
}

void initPeerPeerTable(){
	peer_peertable_head = NULL;
	peer_peertable_tail = NULL;
	peer_peertable_mutex = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(peer_peertable_mutex, NULL);
}

int updatePeerTimeStamp(char* peer_ip, unsigned long timestamp){
	tracker_peer_t * peer = findPeerByIp(peer_ip);
	if(peer != NULL){
		pthread_mutex_lock(peer_table_mutex);
		peer -> last_time_stamp = timestamp;
		pthread_mutex_unlock(peer_table_mutex);
		printPeerTable();
		return 1;
	}
	return -1;
}

void updatePeerTable(tracker_peer_t * oldEntry,tracker_peer_t * newEntry){
	pthread_mutex_lock(peer_table_mutex);
	memcpy(oldEntry->ip,newEntry->ip,IP_LEN);
	oldEntry->last_time_stamp = newEntry->last_time_stamp;
	oldEntry->sockfd = newEntry->sockfd;
	pthread_mutex_unlock(peer_table_mutex);
	printPeerTable();
}

void appendPeerTable(tracker_peer_t * newEntry){
	pthread_mutex_lock(peer_table_mutex);
	if(peertable_head == NULL){
		peertable_head = newEntry;
		peertable_tail = newEntry;
	}else{
		peertable_tail -> next = newEntry;
		peertable_tail = newEntry;
	}
	pthread_mutex_unlock(peer_table_mutex);
	printPeerTable();
}

// if not found return NULL
tracker_peer_t * findPeerByIp(char * ip){
	pthread_mutex_lock(peer_table_mutex);
	tracker_peer_t * itr = peertable_head;
	while(itr != NULL){
		if(strcmp(itr -> ip, ip) == 0){
			pthread_mutex_unlock(peer_table_mutex);
			return itr;
		}
		itr = itr -> next;
	}
	pthread_mutex_unlock(peer_table_mutex);
	printPeerTable();
	return itr;
}


tracker_peer_t* deletePeerEntryByIp(char*ip){
	printf("deletePeerEntryByIp %s\n",ip);
	pthread_mutex_lock(peer_table_mutex);
	tracker_peer_t * itr = peertable_head;
	if(itr == NULL) return NULL;
	tracker_peer_t * pre = NULL,*cur = NULL;
	while(itr != NULL){
		if(strcmp(ip,itr->ip) ==0){
			if(itr == peertable_head){
				cur = itr;
				peertable_head = itr->next;
				if(peertable_head == NULL)
					peertable_tail = NULL;
			}else{
				cur = itr;
				pre->next = itr->next;
				if(itr == peertable_tail)
					peertable_tail = pre;
			}
			free(cur);
			break;
		}
		pre = itr;
		itr = itr -> next;
	}
	pthread_mutex_unlock(peer_table_mutex);
	printPeerTable();
	if (pre == NULL) return NULL;
	else return pre->next;
}
tracker_peer_t* deletePeerEntryBySockfd(int sockfd){
	printf("deletePeerEntryBySockfd %d\n",sockfd);
	pthread_mutex_lock(peer_table_mutex);
	tracker_peer_t * itr = peertable_head;
	if(itr == NULL) return NULL;
	tracker_peer_t * pre = NULL,*cur = NULL;
	while(itr != NULL){
		if(sockfd == itr->sockfd){
			if(itr == peertable_head){
				cur = itr;
				peertable_head = itr->next;
				if(peertable_head == NULL)
					peertable_tail = NULL;
			}else{
				cur = itr;
				pre->next = itr->next;
				if(itr == peertable_tail)
					peertable_tail = pre;
			}
			free(cur);
			break;
		}
		pre = itr;
		itr = itr -> next;
	}
	pthread_mutex_unlock(peer_table_mutex);
	printPeerTable();
	if (pre == NULL) return NULL;
	else return pre->next;
}


void deleteDeadPeers(long currentTime){
	pthread_mutex_lock(peer_table_mutex);
	tracker_peer_t * prev = NULL;
	tracker_peer_t * itr = peertable_head;
	while(itr != NULL){
		if(itr -> last_time_stamp + ALIVE_TIMEOUT < currentTime){
			//time out occurs
			if(prev == NULL){
				//itr is head
				peertable_head = itr -> next;
				free(itr);
				if(peertable_head == NULL)
					peertable_tail = NULL;
			}else{
				prev -> next = itr -> next;
				free(itr);
			}
			printf("TIME OUT! deleted peer with ip %s\n", itr -> ip);
		}
		prev = itr;
		itr = itr -> next;
	}
	pthread_mutex_unlock(peer_table_mutex);
	printPeerTable();
}


// //send peer appendPeerTable
// // convert l-l to array
// int sendPeertable(int connfd){
// 	//compute ll size
// 	tracker_peer_t * itr = peertable_head;
// 	int i = 0;
// 	while(itr != peertable_tail){
// 		i++;
// 		itr = itr -> next;
// 	}
// 	if(peertable_tail != NULL)
// 		i++;
// 	//malloc 
// 	tracker_peer_t * peerArray = malloc(i * sizeof(tracker_peer_t));
// 	//memcpy each ll entry into array
// 	itr = peertable_head;
// 	while(itr != peertable_tail){
// 		if(itr != NULL){

// 			itr = itr -> next;
// 		}
// 	}
// }



void printPeerTable(){
	pthread_mutex_lock(peer_table_mutex);
	printf("===========peer table===========\n");
	tracker_peer_t * itr = peertable_head;
	while(itr != NULL){
		printf("peer ip:%s\tlast-time-stamp:%lu\tsockfd:%d\n", itr -> ip, itr -> last_time_stamp,itr->sockfd);
		itr = itr -> next;
	}
	printf("================================\n");
	pthread_mutex_unlock(peer_table_mutex);
}
