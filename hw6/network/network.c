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
#define NETWORK_WAITTIME 15

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
int EXIT_SIG = 0;
int routingInited = 0;


/**************************************************************/
//implementation network layer functions
/**************************************************************/

//This function is used to for the SNP process to connect to the local ON process on port OVERLAY_PORT.
//TCP descriptor is returned if success, otherwise return -1.
int connectToOverlay() { 
	int sock;
	struct sockaddr_in server;

	// Create socket
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		printf("Could not create socket\n");
		return -1;
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
	int nodeNum = topology_getNodeNum();
	dv_entry_t* routeInfo;
	snp_pkt_t* pkt = (snp_pkt_t*)malloc(sizeof(snp_pkt_t));

	pkt->header.src_nodeID = topology_getMyNodeID();
	pkt->header.dest_nodeID = BROADCAST_NODEID;
	pkt->header.length = 0;
	pkt->header.type = ROUTE_UPDATE;
	memcpy(pkt->data, &nodeNum, sizeof(int));
  pthread_mutex_lock(dv_mutex);
	routeInfo = getRouteInfo(nodeNum);
  memcpy((pkt->data + sizeof(int) / sizeof(char))
    , routeInfo, nodeNum * (sizeof(routeupdate_entry_t)));
	pthread_mutex_unlock(dv_mutex);

	printf("%s: ON\n", __func__);
	while(!EXIT_SIG) {
	  //printf("%s: going to send\n", __func__);
		overlay_sendpkt(BROADCAST_NODEID, pkt, overlay_conn);
		//printf("%s: sent\n", __func__);
		sleep(ROUTEUPDATE_INTERVAL);
		//printf("%s: wake\n", __func__);
	}
	printf("%s: OFF\n", __func__);
	free(routeInfo);
	free(pkt);
	return 0;
}

dv_entry_t* getRouteInfo(int nodeNum) {
  //printf("%s: here\n", __func__);
  int myNodeId = topology_getMyNodeID(), j;
  for(j = 0; (dv+j) != NULL; j++) {
    if(dv[j].nodeID == myNodeId) {
      return dv[j].dvEntry;
    }
  }
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
	snp_pkt_t* pkt = (snp_pkt_t*)malloc(sizeof(snp_pkt_t));
	printf("%s: ON\n", __func__);
	while(!EXIT_SIG) {
	  printf("%s: waiting to get a pkt\n", __func__);
	  while(overlay_recvpkt(pkt, overlay_conn) >= 0){
      // printf("%s: get a pkt\n", __func__);
	    if(pkt->header.type == ROUTE_UPDATE) {
        if(routeUpdateHandler(pkt) != 1) {
	        fprintf(stderr, "err in file %s func %s line %d: routeUpdateHandler err.\n"
		        , __FILE__, __func__, __LINE__); 
        }
//	    } else if (pkt->header.type == SNP) {
	    } else if (1 || pkt->header.type == SNP) {
	      if(pkt->header.dest_nodeID == myNodeId) {
	        if(forwardsegToSRT(transport_conn, pkt->header.dest_nodeID, (seg_t*)pkt->data) != 1) {
		        fprintf(stderr, "err in file %s func %s line %d: forwardsegToSRT err.\n"
			        , __FILE__, __func__, __LINE__); 
	        } else {
	          printf("%s: seg forwarded to srt\n", __func__);
	        }
	      } else {
	        if(forwardHandler(pkt) != 1) {
		        fprintf(stderr, "err in file %s func %s line %d: forwardHandler err.\n"
			        , __FILE__, __func__, __LINE__); 
	        }
	      }
	    } else {
		    fprintf(stderr, "err in file %s func %s line %d: unkown pkt type %d.\n"
			    , __FILE__, __func__, __LINE__, pkt->header.type);
	    }
		  //printf("Routing: received a packet from neighbor %d\n", pkt->header.src_nodeID);
		}
    fprintf(stderr, "err in file %s func %s line %d: overlay_recvpkt err.\n"
	    , __FILE__, __func__, __LINE__); 
	  break;
	}
	printf("%s: OFF\n", __func__);
	close(overlay_conn);
	overlay_conn = -1;
	pthread_exit(NULL);
}

int forwardHandler(snp_pkt_t* pkt) {
  //printf("%s: here\n", __func__);
  int nextNodeId = routingtable_getnextnode(routingtable, pkt->header.dest_nodeID);
  if(nextNodeId == -1) {
    fprintf(stderr, "err in file %s func %s line %d: routing table get nextNodeId %d err.\n"
      , __FILE__, __func__, __LINE__, pkt->header.dest_nodeID); 
    return -1;
  } else {
    overlay_sendpkt(nextNodeId, pkt, overlay_conn);
    return 1;
  }
}

int routeUpdateHandler(snp_pkt_t* pkt) {
  //printf("%s: here\n", __func__);
  int myNodeId = topology_getMyNodeID(), i, j;
  pkt_routeupdate_t* routeupdate = (pkt_routeupdate_t*)pkt->data;
  unsigned int nodeNum = routeupdate->entryNum;
  routeupdate_entry_t* entry;
  dv_entry_t* dvEntry;
  
  //printf("%s: nodeNum is %d\n", __func__, nodeNum);
  // step 1, update that particular entry
  //printf("%s: step 1\n", __func__);
  pthread_mutex_lock(dv_mutex);
  for(j = 0; (dv+j) != NULL; j++) {
    //printf("%s: row %d in dvEntry, nid is %d\n", __func__, j, dv[j].nodeID);
    if(dv[j].nodeID == pkt->header.src_nodeID) {
      for(i = 0; i < nodeNum; i++) {
        //printf("%s: row %d, col %d in dvEntry\n", __func__, j, i);
        // when i = 0, it skip the nodeNum and first id
        // when i > 0, it skip this cost the next node id
        // dv->dvEntry[i].cost = *(pkt->data + 2 + 2 * i);
        entry = routeupdate->entry;
        dv[j].dvEntry[i].cost = entry[i].cost;
      }
      break;
    }
  }
  pthread_mutex_unlock(dv_mutex);
  
  // step 2, update my own entry and update routing table
  //printf("%s: step 2\n", __func__);
  pthread_mutex_lock(routingtable_mutex);
  for(j = 0; (dv+j) != NULL; j++) {
    //printf("%s: row %d in dvEntry, nid is %d\n", __func__, j, dv[j].nodeID);
    if(dv[j].nodeID == myNodeId) {
      for(i = 0; i < nodeNum; i++) {
        //printf("%s: row %d, col %d in dvEntry\n", __func__, j, i);
        dvEntry = dv[j].dvEntry;
        //if(dvEntry[i].nodeID != myNodeId) {
          int newCost = dvtable_getcost(dv, myNodeId, pkt->header.src_nodeID) 
            + dvtable_getcost(dv, pkt->header.src_nodeID, dvEntry[i].nodeID);
          //printf("%s: oldCost %d, newCost %d \n", __func__, dvEntry[i].cost, newCost);
          if(dvEntry[i].cost > newCost) {
          //if(routingInited == 0 || dvEntry[i].cost > newCost) {
            //printf("%s: routingtable update dest %d next %d:)\n", __func__, dvEntry[i].nodeID, pkt->header.src_nodeID);
            dvtable_setcost(dv, myNodeId, pkt->header.src_nodeID, newCost);
            routingtable_setnextnode(routingtable, dvEntry[i].nodeID, pkt->header.src_nodeID);
          }
        //} else if(routingInited == 0) {
          //routingtable_setnextnode(routingtable, myNodeId, myNodeId);
        //}
      }
      //routingInited = 1;
      break;
    }
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
  //printf("%s: here\n", __func__);
	seg_t* seg = (seg_t*)malloc(sizeof(seg_t));
	snp_pkt_t* pkt = (snp_pkt_t*)malloc(sizeof(snp_pkt_t));
	int* destNode = (int*)malloc(sizeof(int));
	int srvconn = server_socket_setup(NETWORK_PORT);
	int myNodeId = topology_getMyNodeID();
	struct sockaddr_in client_addr;
	socklen_t length = sizeof(client_addr);
	
	while (!EXIT_SIG) {
    transport_conn = accept(srvconn, (struct sockaddr*) &client_addr, &length);
    if (transport_conn < 0) {
	    fprintf(stderr, "err in file %s func %s line %d: accept err.\n"
		    , __FILE__, __func__, __LINE__); 
		    exit(1);
    }
    printf("%s: waiting seg from srt\n", __func__);
		while (getsegToSend(transport_conn, destNode, seg) >= 0) {
		  printf("%s: get seg from srt type %d, dest %d\n", __func__, seg->header.type, *destNode);
		  pkt->header.src_nodeID = myNodeId;
		  pkt->header.dest_nodeID = *destNode;
		  pkt->header.length = sizeof(seg_t);
		  pkt->header.type = SNP;
		  memcpy(pkt->data, seg, pkt->header.length);
		  pthread_mutex_lock(routingtable_mutex);
		  routingtable_getnextnode(routingtable, *destNode);
		  pthread_mutex_unlock(routingtable_mutex);
			overlay_sendpkt(*destNode, pkt, overlay_conn);
		}
	}
	free(seg);
	free(destNode);
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
	printf("%s: wake!!!!!!!!!!\n", __func__);
	routingtable_print(routingtable);

	//wait connection from SRT process
	printf("waiting for connection from SRT process\n");
	waitTransport(); 

}


