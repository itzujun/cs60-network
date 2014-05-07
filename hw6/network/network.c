//FILE: network/network.c
//
//Description: this file implements network layer process  
//
//Date: April 29,2008



#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <strings.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <assert.h>
#include <sys/utsname.h>
#include <pthread.h>
#include <unistd.h>

#include "../common/constants.h"
#include "../common/pkt.h"
#include "../common/seg.h"
#include "../topology/topology.h"
#include "network.h"
#include "nbrcosttable.h"
#include "dvtable.h"
#include "routingtable.h"

//network layer waits this time for establishing the routing paths 
#define NETWORK_WAITTIME 60

/**************************************************************/
//delare global variables
/**************************************************************/
int overlay_conn; 			//connection to the overlay
int transport_conn;			//connection to the transport
nbr_cost_entry_t* nct;			//neighbor cost table
dv_t* dv;				//distance vector table
pthread_mutex_t* dv_mutex;		//dvtable mutex
routingtable_t* routingtable;		//routing table
pthread_mutex_t* routingtable_mutex;	//routingtable mutex


/**************************************************************/
//implementation network layer functions
/**************************************************************/

//This function is used to for the SNP process to connect to the local ON process on port OVERLAY_PORT.
//TCP descriptor is returned if success, otherwise return -1.
int connectToOverlay() { 
	int sock;
	char* recv_str;
	struct sockaddr_in server;

	// Create socket
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		printf("Could not create socket\n");
	}
	
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

	if((host = gethostbyname("localhost")) == NULL) {
		fprintf(stderr, "err in file %s func %s line %d: gethostbyname err.\n"
			, __FILE__, __func__, __LINE__); 
		exit(1);
	}

  memcpy(&server.sin_addr.s_addr, host->h_addr_list[0], sizeof(struct in_addr)); 
	server.sin_family = AF_INET;
	server.sin_port = htons(port);

	return server;
}

//This thread sends out route update packets every ROUTEUPDATE_INTERVAL time
//The route update packet contains this node's distance vector. 
//Broadcasting is done by set the dest_nodeID in packet header as BROADCAST_NODEID
//and use overlay_sendpkt() to send the packet out using BROADCAST_NODEID address.
void* routeupdate_daemon(void* arg) {
	//put your code here
	int nbrNum = topology_getNbrNum(), nodeNum = topology_getNodeNUm(), i;
	int* nodeIdArray = topology_getNodeArray();
	int* routeInfo;
	snp_pkt_t* pkt = (snp_pkt_t*)malloc(sizeof(snp_pkt_t));

	pkt->header.src_nodeID = topology_getMyNodeID();
	pkt->header.dest_nodeID = BROADCAST_NODEID;
	pkt->header.length = 0;
	pkt->header.type = ROUTE_UPDATE;
	memcpy(dataPtr, &nodeNum, sizeof(int));
	routeInfo = getRouteInfo(nodeNum);
  memcpy((pkt->data + sizeof(int) / sizeof(char))
    , routeInfo, nodeNum * (2 * sizeof(int));

	printf("%s: ON\n", __func__);
	while(!EXIT_SIG) {
	  // printf("%s: updating routeinfo\n", __func__);
		overlay_sendpkt(BROADCAST_NODEID, pkt, overlay_conn);
		sleep(ROUTEUPDATE_INTERVAL);
	}
	printf("%s: OFF\n", __func__);
	free(routeInfo);
	free(pkt);
	return 0;
}

int* getRouteInfo(int nodeNum) {
  int myNodeId = topology_getMyNodeID();
  pthread_mutex_lock(dv_mutex);
  while(dv != NULL) {
    if(dv->nodeID == myNodeId) {
      return dv.dvEntry[i];
    }
    dv++;
  }
  pthread_mutex_unlock(dv_mutex);
  fprintf(stderr, "err in file %s func %s line %d: cannot not get entries.\n"
    , __FILE__, __func__, __LINE__); 
  return NULL;
}

//This thread handles incoming packets from the ON process.
//It receives packets from the ON process by calling overlay_recvpkt().
//If the packet is a SNP packet and the destination node is this node, forward the packet to the SRT process.
//If the packet is a SNP packet and the destination node is not this node, forward the packet to the next hop according to the routing table.
//If this packet is an Route Update packet, update the distance vector table and the routing table. 
void* pkthandler(void* arg) {
  int myNodeId = topology_getMyNodeID();
	snp_pkt_t pkt;
	printf("%s: ON\n", __func__);
	while(!EXIT_SIG) {
	  overlay_recvpkt(&pkt, overlay_conn);
	  if(pkt->header.type == ROUTE_UPDATE) {
      if(routeUpdateHandler(pkt) != 1) {
	      fprintf(stderr, "err in file %s func %s line %d: forwardHandler err.\n"
		      , __FILE__, __func__, __LINE__); 
      }
	  } else if (pkt->header.type == ROUTE_UPDATE) {
	    if(pkt->heade.dest_nodeID == myNodeId) {
	      if(forwardsegToSRT(transport_conn, pkt->heade.dest_nodeID, pkt->data) != 1) {
		      fprintf(stderr, "err in file %s func %s line %d: forwardHandler err.\n"
			      , __FILE__, __func__, __LINE__); 
	      }
	    } else {
	      if(forwardHandler(pkt) != 1) {
		      fprintf(stderr, "err in file %s func %s line %d: forwardHandler err.\n"
			      , __FILE__, __func__, __LINE__); 
	      }
	    }
	  } else {
		  fprintf(stderr, "err in file %s func %s line %d: pkt type err.\n"
			  , __FILE__, __func__, __LINE__); 
	  }
		printf("Routing: received a packet from neighbor %d\n",pkt.header.src_nodeID);
	}
	printf("%s: OFF\n", __func__);
	close(overlay_conn);
	overlay_conn = -1;
	pthread_exit(NULL);
}

int forwardHandler(snp_pkt_t pkt) {
  int nextNodeId = routingtable_getnextnode(pkt->heade.dest_nodeID);
  if(nextNodeId == -1) {
    fprintf(stderr, "err in file %s func %s line %d: routing table get nextNodeId err.\n"
      , __FILE__, __func__, __LINE__); 
    return -1;
  } else {
    overlay_sendpkt(nextNodeId, pkt, overlay_conn);
    return 1;
  }
}

int routeUpdateHandler(snp_pkt_t pkt) {
  int myNodeId = topology_getMyNodeID(), nodeID, cost, i, j;
  int* routeInfo = pkt->data;
  int nodeNum = *(routeInfo), nbrNum = topology_getNbrNum();
  
  // step 1, update that particular entry
  pthread_mutex_lock(dv_mutex);
  while(dv != NULL) {
    if(dv->nodeID == pkt->src_nodeID) {
      for(i = 0; i < nodeNum; i++) {
        // when i = 0, it skip the nodeNum and first id
        // when i > 0, it skip this cost the next node id
        dv.dvEntry[i].cost = *(routeInfo + 2 + 2 * i);
        memcpy(routeInfo++, &dv.dvEntry[i].nodeID, sizeof(int));
        memcpy(routeInfo++, &, sizeof(int));
      }
      break;
    }
    dv++;
  }
  pthread_mutex_unlock(dv_mutex);
  
  // step 2, update my own entry and update routing table
  pthread_mutex_lock(routingtable_mutex);
  while(dv != NULL) {
    if(dv->nodeID == myNodeId) {
      for(i = 0; i < nodeNum; i++) {
        int newCost = dvtable_getcost(dv, myNodeId, dv.dvEntry[i].nodeID) 
          + dvtable_getcost(dv, dv.dvEntry[i].nodeID, pkt->dest_nodeID);
        if(dv.dvEntry[i].cost > newCost) {
          dvtable_setcost(dv, myNodeId, pkt->dest_nodeID);
          routingtable_setnextnode(routingtable, pkt->dest_nodeID, dv.dvEntry[i].nodeID);
        }
      }
      break;
    }
    dv++;
  }
  pthread_mutex_unlock(routingtable_mutex);
  
  return 1;
}

//This function stops the SNP process. 
//It closes all the connections and frees all the dynamically allocated memory.
//It is called when the SNP process receives a signal SIGINT.
void network_stop() {
	close(overlay_conn);
	EXIT_SIG = 1;
	//add your code here
}

//This function opens a port on NETWORK_PORT and waits for the TCP connection from local SRT process.
//After the local SRT process is connected, this function keeps receiving sendseg_arg_ts which contains the segments and their destination node addresses from the SRT process. The received segments are then encapsulated into packets (one segment in one packet), and sent to the next hop using overlay_sendpkt. The next hop is retrieved from routing table.
//When a local SRT process is disconnected, this function waits for the next SRT process to connect.
void waitTransport() {
	seg_t* seg = (seg_t*)malloc(sizeof(seg_t));
	snp_pkt_t* pkt = (snp_pkt_t*)malloc(sizeof(snp_pkt_t));
	int* destNode = (int*)malloc(sizeof(int));
	int srvconn = server_socket_setup(NETWORK_PORT), i;
	int nodeNum = topology_getNbrNum(), myNodeId = topology_getMyNodeID();
	int* nodeIdArray = topology_getNbrArray();
	struct sockaddr_in client_addr;
	socklen_t length = sizeof(client_addr);
	
	while (!EXIT_SIG) {
    network_conn = accept(srvconn, (struct sockaddr*) &client_addr, &length);
    if (network_conn < 0) {
	    fprintf(stderr, "err in file %s func %s line %d: accept err.\n"
		    , __FILE__, __func__, __LINE__); 
		    exit(1);
    }
		while (getsegToSend(seg, destNode, network_conn) < 0) {
		  pkt->header.src_nodeID = myNodeId;
		  pkt->header.dest_nodeID = destNode;
		  pkt->header.length = sizeof(seg_t);
		  pkt->header.type = SNP;
		  memcpy(pkt->data, seg, pkt->header.length);
		  pthread_mutex_lock(routingtable_mutex);
		  routingtable_getnextnode(routingtable, destNode);
		  pthread_mutex_unlock(routingtable_mutex);
		  //@TODO: get from routing table
			overlay_sendpkt(destNode, pkt, overlay_conn);
		}
	}
}

int main(int argc, char *argv[]) {
	printf("network layer is starting, pls wait...\n");

	//initialize global variables
	nct = nbrcosttable_create();
	dv = dvtable_create();
	dv_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(dv_mutex,NULL);
	routingtable = routingtable_create();
	routingtable_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(routingtable_mutex,NULL);
	overlay_conn = -1;
	transport_conn = -1;

	nbrcosttable_print(nct);
	dvtable_print(dv);
	routingtable_print(routingtable);

	//register a signal handler which is used to terminate the process
	signal(SIGINT, network_stop);

	//connect to local ON process 
	overlay_conn = connectToOverlay();
	if(overlay_conn<0) {
		printf("can't connect to overlay process\n");
		exit(1);		
	}
	
	//start a thread that handles incoming packets from ON process 
	pthread_t pkt_handler_thread; 
	pthread_create(&pkt_handler_thread,NULL,pkthandler,(void*)0);

	//start a route update thread 
	pthread_t routeupdate_thread;
	pthread_create(&routeupdate_thread,NULL,routeupdate_daemon,(void*)0);	

	printf("network layer is started...\n");
	printf("waiting for routes to be established\n");
	sleep(NETWORK_WAITTIME);
	routingtable_print(routingtable);

	//wait connection from SRT process
	printf("waiting for connection from SRT process\n");
	waitTransport(); 

}


