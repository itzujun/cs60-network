//FILE: network/network.c
//
//Description: this file implements SNP process  
//
//Date: April 29,2008



#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <assert.h>
#include <sys/utsname.h>
#include <pthread.h>
#include <unistd.h>

#include "../common/constants.h"
#include "../common/pkt.h"
#include "../topology/topology.h"
#include "network.h"

/**************************************************************/
//declare global variables
/**************************************************************/
int overlay_conn; 		//connection to the overlay


/**************************************************************/
//implementation network layer functions
/**************************************************************/


//this function is used to for the SNP process to connect to the local ON process on port OVERLAY_PORT
//connection descriptor is returned if success, otherwise return -1
int connectToOverlay() { 
	int sock;
	char* recv_str;
	struct sockaddr_in server;

	// Create socket
	sock = create_socket();

	// Configure server info
	server = config_server(OVERLAY_PORT);

	// Connect to remote server
	if (connect(sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
		perror("connect failed. Error");
		return -1;
	}
	return sock;
}

struct sockaddr_in config_server(int port) {
	struct sockaddr_in server;
	struct hostent* host =NULL;

	if((host = gethostbyname(hostname)) == NULL) {
		fprintf(stderr, "err in file %s func %s line %d: gethostbyname err.\n"
			, __FILE__, __func__, __LINE__); 
		return -1;
	}

	server.sin_addr.s_addr = *((struct in_addr*)host->h_addr_list[0]);
	server.sin_family = AF_INET;
	server.sin_port = htons(port);

	return server;
}

//This thread sends out route update packets every ROUTEUPDATE_INTERVAL time
//In this lab this thread only broadcasts empty route update packets to all the neighbors, broadcasting is done by set the dest_nodeID in packet header as BROADCAST_NODEID
void* routeupdate_daemon(void* arg) {
	//put your code here
	int nodeNum = topology_getNbrNum(), i;
	int* nodeIdArray = topology_getNodeArray();
	snp_pkt_t* pkt = (snp_pkt_t*)malloc(sizeof(snp_pkt_t));

	pkt->header.src_nodeID = topology_getMyNodeID();
	pkt->header.dest_nodeID = BROADCAST_NODEID;
	pkt->header.length = 0;
	pkt->header.type = ROUTE_UPDATE;
	while(1) {
		overlay_sendpkt(BROADCAST_NODEID, pkt, overlay_conn);
		sleep(ROUTEUPDATE_INTERVAL);
	}
	free(pkt);
	return 0;
}

//this thread handles incoming packets from the ON process
//It receives packets from the ON process by calling overlay_recvpkt()
//In this lab, after receiving a packet, this thread just outputs the packet received information without handling the packet 
void* pkthandler(void* arg) {
	snp_pkt_t pkt;
	while(overlay_recvpkt(&pkt,overlay_conn)>0) {
		printf("Routing: received a packet from neighbor %d\n",pkt.header.src_nodeID);
	}
	close(overlay_conn);
	overlay_conn = -1;
	pthread_exit(NULL);
}

//this function stops the SNP process 
//it closes all the connections and frees all the dynamically allocated memory
//it is called when the SNP process receives a signal SIGINT
void network_stop() {
	close(overlay_conn);
	//add your code here
}


int main(int argc, char *argv[]) {
	printf("network layer is starting, pls wait...\n");

	//initialize global variables
	overlay_conn = -1;

	//register a signal handler which will kill the process
	signal(SIGINT, network_stop);

	//connect to overlay
	overlay_conn = connectToOverlay();
	if(overlay_conn<0) {
		printf("can't connect to ON process\n");
		exit(1);		
	}
	
	//start a thread that handles incoming packets from overlay
	pthread_t pkt_handler_thread; 
	pthread_create(&pkt_handler_thread,NULL,pkthandler,(void*)0);

	//start a route update thread 
	pthread_t routeupdate_thread;
	pthread_create(&routeupdate_thread,NULL,routeupdate_daemon,(void*)0);	

	printf("network layer is started...\n");

	//sleep forever
	while(1) {
		sleep(60);
	}
}


