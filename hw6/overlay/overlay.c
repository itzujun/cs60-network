//FILE: overlay/overlay.c
//
//Description: this file implements a ON process 
//A ON process first connects to all the neighbors and then starts listen_to_neighbor threads each of which keeps receiving the incoming packets from a neighbor and forwarding the received packets to the SNP process. Then ON process waits for the connection from SNP process. After a SNP process is connected, the ON process keeps receiving sendpkt_arg_t structures from the SNP process and sending the received packets out to the overlay network. 
//
//Date: April 28,2008


#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <sys/utsname.h>
#include <assert.h>
#include <strings.h>
#include "../common/constants.h"
#include "../common/pkt.h"
#include "overlay.h"
#include "../topology/topology.h"
#include "neighbortable.h"

//you should start the ON processes on all the overlay hosts within this period of time
#define OVERLAY_START_DELAY 10
#define BUFFER_SIZE 1024

/**************************************************************/
//declare global variables
/**************************************************************/

//declare the neighbor table as global variable 
nbr_entry_t* nt; 
//declare the TCP connection to SNP process as global variable
int network_conn; 
int EXIT_SIG = 0;
int keeplistenning = 1;


/**************************************************************/
//implementation overlay functions
/**************************************************************/

// This thread opens a TCP port on CONNECTION_PORT and waits for the incoming connection from all the neighbors that have a larger node ID than my nodeID,
// After all the incoming connections are established, this thread terminates 
void* waitNbrs(void* arg) {
	int nodeId, myNodeId = topology_getMyNodeID();
	int nodeNum = topology_getNbrNum(), i, nbrToConNum = 0;
	int srvconn = server_socket_setup(CONNECTION_PORT);
	if(srvconn == -1) {
		fprintf(stderr, "err in file %s func %s line %d: server_socket_setup err.\n"
			, __FILE__, __func__, __LINE__); 
	}

  printf("%s: getNbrNum %d\n", __func__, nodeNum);
  for(i = 0; i < nodeNum; i++) {
    if(nt[i].nodeID > myNodeId)
      nbrToConNum++;
  }
	while(!EXIT_SIG && nbrToConNum-- > 0) {
		struct sockaddr_in client_addr;
		socklen_t length = sizeof(client_addr);

		// connect with client
		printf("%s: waiting..\n", __func__);
		int client_conn = accept(srvconn, (struct sockaddr*) &client_addr,
			&length);
		if (client_conn < 0) {
			fprintf(stderr, "err in file %s func %s line %d: accept err.\n"
				, __FILE__, __func__, __LINE__); 
		}
		
    nodeId = topology_getNodeIDfromip(&(client_addr.sin_addr));
		if(nodeId == -1) {
			fprintf(stderr, "err in file %s func %s line %d: topology_getNodeIDfromip err.\n"
				, __FILE__, __func__, __LINE__); 
		}
		printf("%s: get nbrid %d sock %d\n", __func__, nodeId, client_conn);
		if(nodeId > myNodeId) {
			if(nt_addconn(nt, nodeId, client_conn) == -1) {
				fprintf(stderr, "err in file %s func %s line %d: nt_addconn err.\n"
					, __FILE__, __func__, __LINE__); 
			}		
		}
	}
	printf("%s: finish\n", __func__);
	return NULL;
}

int server_socket_setup(int port) {
	// build server Socket
	int server_socket = socket(PF_INET, SOCK_STREAM, 0);
	if (server_socket < 0) {
		printf("Socket create failed.\n");
		return EXIT_FAILURE;
	} else {
		int opt = 1;
		setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	}

	// set server configuration
	struct sockaddr_in server_addr;
	bzero(&server_addr, sizeof(server_addr));

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htons(INADDR_ANY);
	server_addr.sin_port = htons(port);

	// bind the port
	if (bind(server_socket, (struct sockaddr*) &server_addr, sizeof(server_addr))) {
		printf("Bind port %d failed.\n", port);
		return -1;
	}

	// listening
	if (listen(server_socket, LISTEN_QUEUE_LENGTH)) {
		printf("Server Listen Failed!");
		return EXIT_FAILURE;
	}

	return server_socket;
}

// This function connects to all the neighbors that have a smaller node ID than my nodeID
// After all the outgoing connections are established, return 1, otherwise return -1
int connectNbrs() {
	int myNodeId = topology_getMyNodeID(), nbrIdx = 0;
	int nodeNum = topology_getNbrNum();
	int sock;
	struct sockaddr_in server;

	while(!EXIT_SIG && nodeNum-- > 0) {
	
	  if(nt[nbrIdx].nodeID >= myNodeId) 
	    continue;
	    
	  sock = socket(AF_INET, SOCK_STREAM, 0);
	  if (sock == -1) 
		  printf("Could not create socket\n");
	  server = config_server(nt[nbrIdx].nodeIP);

		// Connect to remote node
		printf("%s: adding nodeid %d's socket\n", __func__, nt[nbrIdx].nodeID);
		if (connect(sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
			perror("connect failed. Error");
			// printf("%s: ip is %s\n", __func__, inet_ntoa(nt[nodeIdArray[nbrIdx]].nodeIP));
			exit(1);
		}
    
		if(nt_addconn(nt, nt[nbrIdx].nodeID, sock) == -1)  {
			fprintf(stderr, "err in file %s func %s line %d: nt_addconn err.\n"
				, __FILE__, __func__, __LINE__);
			exit(1);
		}
		nbrIdx++;
	}
	printf("%s: finish\n", __func__);
	return 1;
}

struct sockaddr_in config_server(in_addr_t nodeIP) {
	struct sockaddr_in server;
	server.sin_addr.s_addr = nodeIP;
	server.sin_family = AF_INET;
	server.sin_port = htons(CONNECTION_PORT);
	return server;
}

//Each listen_to_neighbor thread keeps receiving packets from a neighbor. It handles the received packets by forwarding the packets to the SNP process.
//all listen_to_neighbor threads are started after all the TCP connections to the neighbors are established 
void* listen_to_neighbor(void* arg) {

	snp_pkt_t* pkt = (snp_pkt_t*)malloc(sizeof(snp_pkt_t));
	while (!EXIT_SIG) {
	  if(!keeplistenning) {
	    sleep(10000);
	    printf("%s: stop listening.\n", __func__);
	    continue;
	  }
		if (recvpkt(pkt, nt[*((int*)arg)].conn) < 0) {
			printf("%s-%d: snp process is down on the other side.\n", __func__, *((int*)arg));
			keeplistenning = 0;
		} else {
		  printf("%s-%d: get a packet type %d.\n", __func__, *((int*)arg), pkt->header.type);
			if(network_conn != -1 && keeplistenning) {
				forwardpktToSNP(pkt, network_conn);
			} else {
			  printf("%s: snp process is not connected, maybe try latter\n", __func__);
			}
		}
	}
	free((int*)arg);
	return 0;
}

//This function opens a TCP port on OVERLAY_PORT, and waits for the incoming connection from local SNP process. After the local SNP process is connected, this function keeps getting sendpkt_arg_ts from SNP process, and sends the packets to the next hop in the overlay network. If the next hop's nodeID is BROADCAST_NODEID, the packet should be sent to all the neighboring nodes.
void waitNetwork() {
	snp_pkt_t* pkt = (snp_pkt_t*)malloc(sizeof(snp_pkt_t));
	int* nextNode = (int*)malloc(sizeof(int));
	int srvconn = server_socket_setup(OVERLAY_PORT), i;
	int nodeNum = topology_getNbrNum();
	int* nodeIdArray = topology_getNbrArray();
	struct sockaddr_in client_addr;
	socklen_t length = sizeof(client_addr);
	
	network_conn = accept(srvconn, (struct sockaddr*) &client_addr, &length);
	if (network_conn < 0) {
		fprintf(stderr, "err in file %s func %s line %d: accept err.\n"
			, __FILE__, __func__, __LINE__); 
			exit(1);
	}

  int keepwaitting = 1;
	while (!EXIT_SIG) {
	  if(!keepwaitting){
	    sleep(10000);
	    printf("%s: stop waiting.\n", __func__);
	    continue;
	  }
		if (getpktToSend(pkt, nextNode, network_conn) < 0) {
		  printf("%s: snp is down\n", __func__);
		  keepwaitting = 0;
			//fprintf(stderr, "err in file %s func %s line %d: recv err.\n"
			//	, __FILE__, __func__, __LINE__); 
		} else if (*nextNode == BROADCAST_NODEID) {
			for(i = 0; i < nodeNum; i++) {
				sendToNeighbor(pkt, nodeIdArray[i]);
			}
		} else {
			sendToNeighbor(pkt, *nextNode);
		}
	}
}

void sendToNeighbor(snp_pkt_t* pkt, int nodeId) {
  int nodeNum = topology_getNbrNum(), i;
  int* nodeIdArray = topology_getNbrArray();
  for(i = 0; i < nodeNum; i++) {
    if(nodeId == nt[i].nodeID){
      if(sendpkt(pkt, nt[i].conn) == -1){
	      printf("%s: snp process is not connected on the other side.\n", __func__);
      } else {
        return;
      }
    }
  }
	free(nodeIdArray);
}

//this function stops the overlay
//it closes all the connections and frees all the dynamically allocated memory
//it is called when receiving a signal SIGINT
void overlay_stop() {
	int nbrNum = topology_getNbrNum(), i;
	for(i = 0; i < nbrNum; i++) {
		close(nt[i].conn);
	}
	close(network_conn);
	nt_destroy(nt);
	EXIT_SIG = 1;
}

int main() {
	//start overlay initialization
	printf("Overlay: Node %d initializing...\n",topology_getMyNodeID());	

	//create a neighbor table
	nt = nt_create();
	//initialize network_conn to -1, means no SNP process is connected yet
	network_conn = -1;
	
	//register a signal handler which is sued to terminate the process
	signal(SIGINT, overlay_stop);

	//print out all the neighbors
	int nbrNum = topology_getNbrNum();
	int i;
	for(i=0;i<nbrNum;i++) {
		printf("Overlay: neighbor %d:%d\n",i+1,nt[i].nodeID);
	}

	//start the waitNbrs thread to wait for incoming connections from neighbors with larger node IDs
	pthread_t waitNbrs_thread;
	pthread_create(&waitNbrs_thread,NULL,waitNbrs,(void*)0);

	//wait for other nodes to start
	sleep(OVERLAY_START_DELAY);
	printf("wake\n");
	
	//connect to neighbors with smaller node IDs
	connectNbrs();

	//wait for waitNbrs thread to return
	pthread_join(waitNbrs_thread,NULL);	

	//at this point, all connections to the neighbors are created
	
	//create threads listening to all the neighbors
	for(i=0;i<nbrNum;i++) {
		int* idx = (int*)malloc(sizeof(int));
		*idx = i;
		pthread_t nbr_listen_thread;
		pthread_create(&nbr_listen_thread,NULL,listen_to_neighbor,(void*)idx);
	}
	printf("Overlay: node initialized...\n");
	printf("Overlay: waiting for connection from SNP process...\n");

	//waiting for connection from  SNP process
	waitNetwork();
}
