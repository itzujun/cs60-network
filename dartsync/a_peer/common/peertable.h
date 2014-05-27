#ifndef PEERTABLE_H
#define PEERTABLE_H
#include "constants.h"
#include <pthread.h>
//Peer-side peer table.
typedef struct _peer_side_peer_t {
	// Remote peer IP address, 16 bytes.
	char ip[IP_LEN];
	//Current downloading file name.
	char file_name[FILE_NAME_MAX_LEN];
	//Timestamp of current downloading file.
	unsigned long file_time_stamp;
	//TCP connection to this remote peer.
	int sockfd;
	//Pointer to the next peer, linked list.
	struct _peer_side_peer_t *next;
} peer_peer_t;



//tracker side peer table
typedef struct _tracker_side_peer_t {
	//Remote peer IP address, 16 bytes.
	char ip[IP_LEN];
	//Last alive timestamp of this peer.
	unsigned long last_time_stamp;
	//TCP connection to this remote peer.
	int sockfd;
	//Pointer to the next peer, linked list.
	struct _tracker_side_peer_t *next;
} tracker_peer_t;

tracker_peer_t * peertable_head;
tracker_peer_t * peertable_tail;
pthread_mutex_t * peer_table_mutex; // mutex for peer table

peer_peer_t * peer_peertable_head;
peer_peer_t * peer_peertable_tail;
pthread_mutex_t * peer_peertable_mutex; // mutex for peer table


//append tracker side peer table
void appendPeerTable(tracker_peer_t * newEntry);
void updatePeerTable(tracker_peer_t * oldEntry,tracker_peer_t*newEntry);
//find peer by ip
tracker_peer_t * findPeerByIp(char * ip);
//init table
void initPeerTable();
//delete alive_timeout peers
void deleteDeadPeers(long currentTime);
tracker_peer_t * deletePeerEntryBySockfd(int sockfd);
tracker_peer_t * deletePeerEntryByIp(char*ip);
//print peer table
void printPeerTable();

int updatePeerTimeStamp(char* pkt, unsigned long timestamp);
void initPeerPeerTable();

// two peer side peer table functions added by junjie
void peer_peertable_add(char* ip, char* name, unsigned long timestamp, int sock);
void peer_peertable_rm(char* ip, char* name);
int peer_peertable_found(char* ip, char* name);
#endif
