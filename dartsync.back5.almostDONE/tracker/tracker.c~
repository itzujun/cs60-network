/*
tracker - main
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
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
#include "tracker.h"
#include "../common/filetable.h"
#include "../common/peertable.h"


int listenfd = -1; // main thread socket
int alivefd = -1; //alive thread socket



void updateFileTable(ptp_peer_t * pkt){
	Node * itr = filetable_head;
	int numOfFiles = pkt -> file_table_size;
	int i = 0;
	if(itr == NULL){
		//append all file table entries to FileTable
		while(i < numOfFiles){
			//foreach file
			Node * newEntry = malloc(sizeof(Node));
			memset(newEntry, 0, sizeof(Node));
			newEntry -> size = pkt -> file_table[i].size;
			memcpy(newEntry -> name, pkt -> file_table[i].name, FILE_NAME_MAX_LEN);
			// newEntry -> timestamp = pkt -> file_table[i].timestamp;
			memcpy(newEntry -> md5, pkt -> file_table[i].md5, MD5_LEN);
			newEntry -> pNext = NULL;
			memset(newEntry -> newpeerip, 0, IP_LEN * MAX_PEER_NUM); //memset to 0
			memcpy(newEntry -> newpeerip, pkt -> file_table[i].newpeerip, IP_LEN);
			newEntry->peer_ip_num=1;
			//append it 
			appendFileEntry(newEntry);
			i++;
		}
		return;
	}else{
		i = 0;
		while(i < numOfFiles){
			//foreach file
			if(pkt->file_table[i].status == UPDATING) {
				i++;
				continue;
			}
			char * file_i_name = pkt -> file_table[i].name;
			Node * file_in_table = findFileEntryByName(file_i_name);
			if(file_in_table == NULL){
				//new file append in file table
				Node * newEntry = malloc(sizeof(Node));
				memset(newEntry, 0, sizeof(Node));
				newEntry -> size = pkt -> file_table[i].size;
				memcpy(newEntry -> name, pkt -> file_table[i].name, FILE_NAME_MAX_LEN);
				//newEntry -> timestamp = pkt -> file_table[i].timestamp;
				memcpy(newEntry -> md5, pkt -> file_table[i].md5, MD5_LEN);
				newEntry -> pNext = NULL;
				memset(newEntry -> newpeerip, 0, MAX_PEER_NUM * IP_LEN); //memset to 0
				memcpy(newEntry -> newpeerip, pkt -> file_table[i].newpeerip, IP_LEN);
				newEntry->peer_ip_num=1;
				//append it 
				appendFileEntry(newEntry);
			}else{
				//check md5
				if(memcmp(pkt -> file_table[i].md5, file_in_table -> md5,MD5_LEN) != 0){
					//update file table entry
					
					updateFileEntry(file_in_table,&pkt -> file_table[i]);
				}else{
					int i=0;
					for(;i<file_in_table->peer_ip_num;i++){
						if(memcmp(file_in_table -> newpeerip[i],pkt -> peer_ip, IP_LEN) == 0){
							break;
						}
					}
					pthread_mutex_lock(file_table_mutex);
					if(i==file_in_table->peer_ip_num){
						printf("add new ip into Node ...\n");
						memcpy(file_in_table -> newpeerip[file_in_table->peer_ip_num], pkt -> peer_ip, IP_LEN);
						file_in_table->peer_ip_num ++;
					}
					pthread_mutex_unlock(file_table_mutex);
				}
			}
			i++;
		}

	}
	//check any file deleted
	itr =filetable_head;
	while(itr){
		int found = 0;
		for(i=0;i<pkt->file_table_size;i++){
			if(strcmp(itr->name,pkt->file_table[i].name)==0){
				found =1;
			}
		}
		if(!found){
			itr = deleteFileEntry(itr->name);
		}else{
			itr=itr->pNext;
		}
	}
	printFileTable();
}


void broadcastFileTable(char * pktFromIp){
	tracker_peer_t * peerItr = peertable_head;
	//prepare broadcast pkt
	ptp_tracker_t * pkt = malloc(sizeof(ptp_tracker_t));
	if(pkt == NULL) return;
	pkt -> interval = INTERVAL;
	pkt -> piece_len = PIECE_LENGTH;
	pkt -> file_table_size = getFileTableSize();
	Node * itr = filetable_head;
	int i = 0;
	pthread_mutex_lock(file_table_mutex);
	while(itr != NULL){
		memcpy(&(pkt -> file_table[i]), itr, sizeof(Node));
		i++;
		itr = itr -> pNext; 
	}
	pthread_mutex_unlock(file_table_mutex);
	//send the pkt to all peers
	while(peerItr != NULL){
		//skip pktFromIP
		if(strcmp(peerItr -> ip, pktFromIp) == 0){
			peerItr = peerItr -> next;
			continue;
		}
		//send ptp_tracker_t pkt
		int ret = sendPkt(peerItr -> sockfd, pkt);
		if(ret<0){
			peerItr = deletePeerEntryBySockfd(peerItr -> sockfd);
			continue;
		}else{
			printf("send broadcast ptp_tracker_t sockfd:%d\tip:%s\tpiece_len:%d\tfile_table_size:%d\n",
			peerItr -> sockfd,peerItr->ip,pkt->piece_len,pkt -> file_table_size);
			peerItr = peerItr -> next;
		}
	}
	free(pkt);
}

unsigned long getCurrentTime(){
	time_t the_time;
    	the_time = time((time_t *)0);
    	return the_time;
}

void alive_handler(void* arg){
	ptp_peer_t* buffer = (ptp_peer_t*)arg;
	if(buffer -> type == KEEPALIVE){
		//update peer table
		unsigned long currentTime = getCurrentTime();
		int a = updatePeerTimeStamp(buffer->peer_ip, currentTime);
		if(a < 1){
			printf("failed to update time stamp of peer with ip %s, peer's dead!\n", buffer -> peer_ip);
		}
	}
}

void *handshake_handler(void* arg){
	ptp_peer_t * pkt = malloc(sizeof(ptp_peer_t));
	tracker_peer_t * current = (tracker_peer_t*) arg;
	//printf("ip %s sockfd %d\n", current -> ip, current -> sockfd);
	//printf("before while loop \n");
	while(1){
		memset(pkt, 0, sizeof(ptp_peer_t));
		if(current == NULL){
			//if current has been deleted
			// printf("fdffdfdf\n");
			printf("one peer down...\n");
			free(pkt);
			pthread_exit(NULL);
		} 
		//printf("sockfd %d\n", current -> sockfd);
		//printf("size %lu\n", sizeof(ptp_peer_t));
		int a = recvPkt(current->sockfd, pkt);
		if(a < 0){
			perror("failed to read from socket in handshake_handler\n");
			free(pkt);
			pthread_exit(NULL);
		}
		/*if(strcmp(pkt -> peer_ip, "") == 0){
			pthread_mutex_unlock(peer_table_mutex);
			continue;
		}*/
//		printf("buffer type %d ip %s\n", pkt -> type, pkt -> peer_ip);
		if(strcmp(pkt -> peer_ip, current -> ip) != 0 || strcmp(pkt -> peer_ip, "") == 0){
			// if current.ip != expected ip
			// printf("fdsdsdsd\n");
			//printf("one peer down...\n");
			//free(pkt);
			//pthread_exit(NULL);
			continue;
		}
		//printf("recv ptp_peer_t ip:%s\tport:%d\ttype:%d\tprotocol len:%d\tfiletablesize:%d\n", pkt -> peer_ip, pkt -> port, pkt -> type, pkt -> protocol_len, pkt -> file_table_size);
		//handle handshake msgs
		if(pkt -> type == FILEUPDATE){
			printf("recv fileupdate pkt from %s\n", pkt -> peer_ip);
			//update file table
			updateFileTable(pkt);
			//broadcast file table to all peers
			broadcastFileTable(pkt->peer_ip);
		}
		if(pkt -> type == KEEPALIVE){
			printf("recv new keep alive msg\n");
			alive_handler(pkt);
		}	
	}
}

void *alive_timeout_handler(void* arg){
	printf("alive timeout thread begins to minitor...\n");
	while(1){
		sleep(ALIVE_POLLING_TIME);
		//check if all peers still alive
		unsigned long currentTime = getCurrentTime();
		printf("current time: %lu\n", currentTime);
		deleteDeadPeers(currentTime);
		//printPeerTable();
	}
}



int main(){
	/*
	char *a = "a";
	printf("pointer size %lu\n", sizeof(a));
	printf("int size %lu\n", sizeof(int));
	printf("node size %lu\n", sizeof(Node));
	printf("sizeof pkt: %lu\n", sizeof(ptp_peer_t));
	printf("ttp pkt size: %lu\n", sizeof(ptp_tracker_t));
	*/
	signal(SIGPIPE, SIG_IGN);
	printf("starting up server...\n");
	printf("begin to init peertable...\n");
	initPeerTable();
	pthread_t alive_timeout_thread;
	pthread_create(&alive_timeout_thread, NULL, alive_timeout_handler, NULL);


	printf("init filetable...\n");
	initFileTable();
	//create sock
	printf("creating socket...\n");
	socklen_t clilen;
	struct sockaddr_in cliaddr, servaddr;
	if((listenfd = socket (AF_INET, SOCK_STREAM, 0)) <0){
		perror("Problem in creating the socket");
	}
	//preparation of the socket address
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(HANDSHAKE_PORT);

	int res = bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
	if(res < 0) {
		perror("error in bind");
		exit(-1);
	}
	res = listen(listenfd, 5);
	if(res < 0)
		perror("error in listen");
	printf("%s\n","Server running...waiting for connections.");
	// char * buffer = malloc(BUF_SIZE);
	ptp_peer_t * p_pkt = malloc(sizeof(ptp_peer_t));
	while(1)
	{
		memset(p_pkt, 0, sizeof(ptp_peer_t));
		clilen = sizeof(cliaddr);
		//accept a connection
		int connfd = accept (listenfd, (struct sockaddr *) &cliaddr, &clilen);
		if (connfd == -1){
			perror("error in accept");
		}
		printf("recv register pkt ...\n");
		recvPkt(connfd, p_pkt);
		//printf("recv ptp_peer_t ip:%s\tport:%d\ttype:%d\tprotocol len:%d\tfiletablesize:%d\n", p_pkt -> peer_ip, p_pkt -> port, 
				//p_pkt -> type, p_pkt -> protocol_len, p_pkt -> file_table_size);
		//handle handshake msgs
		if(p_pkt -> type == REGISTER){
			//insert into peer table
			tracker_peer_t * current = malloc(sizeof(tracker_peer_t));
			memset(current, 0, sizeof(tracker_peer_t));
			memcpy(current -> ip, p_pkt -> peer_ip, IP_LEN); // set ip
			current -> last_time_stamp = getCurrentTime();
			current -> sockfd = connfd;
			current -> next = NULL;
			//append to peertable
			tracker_peer_t* oldEntry = findPeerByIp(current->ip);
			if(oldEntry == NULL){
				appendPeerTable(current);
			}else{
				updatePeerTable(oldEntry, current);
			}
			
			//send back pkt
			ptp_tracker_t * response = malloc(sizeof(ptp_tracker_t));
			if(response == NULL) return -1;
			response -> interval = INTERVAL;
			response -> piece_len = PIECE_LENGTH;
			response -> file_table_size = getFileTableSize();
			Node * itr = filetable_head;
			int i = 0;
			pthread_mutex_lock(file_table_mutex);
			while(itr != NULL){
				memcpy(&(response-> file_table[i]), itr, sizeof(Node));
				i++;
				itr = itr -> pNext; 
			}
			pthread_mutex_unlock(file_table_mutex);

			sendPkt(connfd, response);
			printf("send response file_table_size:%d\n", response -> file_table_size);
			free(response);
			pthread_t handshake; // create new thread
			pthread_create(&handshake, NULL, handshake_handler, current);
			printf("start handshake_handler for ip %s\n",current->ip);
		}
	
	}
}
