#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>

#include "peer.h"
#include "../common/filetable.h"
#include "../common/peertable.h"
#include "../common/pkt.h"
#include "../p2p/p2p.h"
#include "../inotify/inotify_utils.h"

#define MAXLINE 100
#define BASE_PORT 18080

int ptp_conn =-1;
int interval;
int piece_len;
char watchdir[MAXLINE];
char my_ip[IP_LEN];
int needUpdate = 0;

int get_myip(char*myip){
	char* hostname = malloc(MAXLINE);
	bzero(hostname,MAXLINE);
	if(gethostname(hostname, MAXLINE) <0){
		perror("gethostname");
		return -1;
	}
	char*ip = NULL;
	struct hostent* h =NULL;
	if((h=gethostbyname(hostname))==NULL){
		perror("gethostbyname");
		return -1;
	}
	if((ip = inet_ntoa(*((struct in_addr*)h->h_addr_list[0])))== NULL){
		perror("addr_ntoa");
		return -1;
	}
	strcpy(myip,ip);
	strcpy(my_ip,ip);
	return 0;
}

int get_random_port(){
       int port = 0;
       srand(time(NULL));
       port = (BASE_PORT+rand()%(65335-BASE_PORT));
       return port;
}

int connectTracker(){
	char tracker[MAXLINE];
	strcpy(tracker,get_from_config("tracker"));
	struct sockaddr_in servaddr;
	struct hostent *hostInfo;

	hostInfo = gethostbyname(tracker);
	if(hostInfo == NULL) {
		perror("gethostbyname!\n");
		return -1;
	}
	servaddr.sin_family =hostInfo->h_addrtype;
	memcpy((char *) &servaddr.sin_addr.s_addr, hostInfo->h_addr_list[0], hostInfo->h_length);
	servaddr.sin_port = htons(HANDSHAKE_PORT);

	ptp_conn = socket(AF_INET,SOCK_STREAM,0);
	if(ptp_conn<0) {
		return -1;
	}
	if(connect(ptp_conn, (struct sockaddr*)&servaddr, sizeof(servaddr))<0)
		return -1;
	printf("connect tracker  %s port %d\n",tracker,HANDSHAKE_PORT);
	return 0;

}

int registerTracker(){
	ptp_peer_t* ptpp = (ptp_peer_t*)malloc(sizeof(ptp_peer_t));
	if(ptpp == NULL ) return -1;
	bzero(ptpp,sizeof(ptp_peer_t));
	ptpp->type = REGISTER;
	get_myip(ptpp->peer_ip);
	ptpp->port = LISTENING_PORT;

	/*
	char*send_buf = malloc(sizeof(ptp_peer_t));
	if(send_buf == NULL) return;
	memcpy(send_buf,ptpp,sizeof(ptp_peer_t));
	*/
	int res = sendPkt_c(ptp_conn, ptpp);
	if (res < 1) {
		perror("reigster failure");;
		return -1;
	}
	printf("registerTracker type %d peer_ip %s port %d\n",
			ptpp->type,ptpp->peer_ip,ptpp->port);
	free(ptpp);
	return 0;
}
void sigintHanlder(){
	exit(-1);
}

int main(){
	//initialize ft and pt
	signal(SIGINT,sigintHanlder );
	printf("begin to init peertable...\n");
        initPeerPeerTable();
	printf("init filetable...\n");
	initFileTable();
	strcpy(watchdir,get_from_config("watch"));
	printf("watchdir : %s\n",watchdir);
        	
	//connect tracker
	if(connectTracker() <0){
		printf("connectTracker failure\n");
		exit(-1);
	}
	printf("connectTracker successful\n");

	//register to tracker
	if(registerTracker()<0){
		printf("registerTracker failure\n");
		exit(-1);
	}
	printf("registerTracker successful\n");

	//recv register response from tracker
	pthread_t ptp_listening_thread,file_monitor_thread,keep_alive_thread;
	ptp_tracker_t* pt= (ptp_tracker_t*)malloc(sizeof(ptp_tracker_t));
	if(pt == NULL ) return -1;
	int init_done = 0;
	while(1){
		printf("keep recving pkt from tracker...\n");
		
		bzero(pt,sizeof(ptp_tracker_t));
		int res = recvPkt_c(ptp_conn,pt);
		if(res < 0){
			perror("recv failure");
			exit(-1);
		}
		if(pt->interval == 0) continue;
		printf("recv broadcast from tracker file_table_size:%d\n ", pt->file_table_size);
		if(init_done ==0){
			interval = pt->interval;
			piece_len = pt->piece_len;
				//call p2p_listening thread by junjie
			pthread_create(&ptp_listening_thread,NULL,p2p_listening,NULL);
			//start file_monitor thread by mengling
			pthread_create(&file_monitor_thread,NULL,file_monitor,NULL);
			pthread_create(&keep_alive_thread,NULL,keep_alive,NULL);
			init_done = 1;
			printf("init done! ptp_listening,file_monitor,keep_alive\n");
		}
		//start downloading if needed
		int i;
		
		dontUpdate = 1;
		//update peer file table
		pthread_mutex_lock(file_table_mutex);
		for(i=0;i<pt->file_table_size;i++){
		  Node* fte = (Node*)malloc(sizeof(Node));
		  if(fte == NULL) return -1;
			bzero(fte,sizeof(Node));
			memcpy(fte,&pt->file_table[i],sizeof(Node));
			int is_updated = 0;

			Node* p =filetable_head;
			while(p ){
				if(strcmp(p->name,fte->name)==0 && strcmp(p->md5, fte->md5)== 0){
					is_updated = 1;
					break;
				}
				p=p->pNext;
			}

			if(!is_updated){
				Node* newEntry = createNode(p->name,UPDATING);
    				if(newEntry!=NULL){
					appendFileEntryNoLock(newEntry);
				}
				pthread_t ptp_transfer_thread;
				//call p2p_download_start thread by junjie
				pthread_create(&ptp_transfer_thread,NULL,p2p_download_start,(void*)fte);
			}
		}
		pthread_mutex_unlock(file_table_mutex);
		//sendFileUpdate();
		//deleting if needed
		Node* head =filetable_head;
		while(head){
			int found = 0;
			for(i=0;i<pt->file_table_size;i++){
				if(strcmp(head->name,pt->file_table[i].name)==0){
					found =1;
				}
			}
			if(!found){
			  if(remove(head->name) < 0) {
			    perror("remove file fail");
			  }
		          head = deleteFileEntry(head->name);
			}else{
				head=head->pNext;
			}
		}
		dontUpdate = 0;

	}
}


void* ptp_listening(void* arg){
	 int    listenfd,connfd;
	 struct sockaddr_in     servaddr;
	 if( (listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1 ){
		printf("create socket error: %s(errno: %d)\n",strerror(errno),errno);
		pthread_exit(NULL);
	 }
	 memset(&servaddr, 0, sizeof(servaddr));
	 servaddr.sin_family = AF_INET;
	 servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	 servaddr.sin_port = htons(LISTENING_PORT);
	 if( bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1){
		 printf("bind socket error: %s(errno: %d)\n",strerror(errno),errno);
		 pthread_exit(NULL);
	 }
	 if( listen(listenfd, 10) == -1){
		 printf("listen socket error: %s(errno: %d)\n",strerror(errno),errno);
		 pthread_exit(NULL);
	 }
	 while(1){
		 if( (connfd = accept(listenfd, (struct sockaddr*)NULL, NULL)) == -1){
			 printf("accept socket error: %s(errno: %d)",strerror(errno),errno);
			 continue;
		 }
		 pthread_t upload_thread;
		 pthread_create(&upload_thread,NULL,upload,(void*)&connfd);
	 }
}



void*file_monitor(void* arg){
        pthread_t fileupdate_thread;
        pthread_create(&fileupdate_thread,NULL,sendFileUpdate,NULL);
	dontUpdate = 0;
	printf("inotify_watch begins\n");
	inotify_watch(watchdir);
	printf("inotify_watch ends\n");
	return NULL;
}

void*ptp_transfer(void* arg){
	return NULL;
}

void*upload(void* arg){
	return NULL;
}

void* keep_alive(void* arg){
	ptp_peer_t* ptpp;
	ptpp = (ptp_peer_t*)malloc(sizeof(ptp_peer_t));
	if(ptpp == NULL){
		perror("malloc failure");
		pthread_exit(NULL);
	}
	bzero(ptpp,sizeof(ptp_peer_t));

	ptpp->type = KEEPALIVE;
	get_myip(ptpp->peer_ip);
	ptpp->port = LISTENING_PORT;
	while(1){
		int res = sendPkt_c(ptp_conn, ptpp);
		if (res < 0) {
			perror("sending keep-alive failure");;
			pthread_exit(NULL);
		}
		printf("send ptp_peer_t type: %d\tpeer_ip:%s\tport:%d\tfile_table_size:%d\n",ptpp->type,
				ptpp->peer_ip,ptpp->port,ptpp->file_table_size);
		sleep(interval);
	}
	free(ptpp);
}

void *sendFileUpdate(void* arg){
	
	ptp_peer_t* ptpp;
	ptpp = (ptp_peer_t*)malloc(sizeof(ptp_peer_t));
	if(ptpp == NULL){
		perror("malloc failure");
		return NULL;
	}
	bzero(ptpp,sizeof(ptp_peer_t));

	ptpp->type = FILEUPDATE;
	get_myip(ptpp->peer_ip);
	ptpp->port = LISTENING_PORT;

	while(1){
		sleep(FILE_UPDATE_INTERVAL);
		if(dontUpdate) continue;
		if(!needUpdate) continue;
		ptpp->file_table_size = getFileTableSize();

		int i;
		pthread_mutex_lock(file_table_mutex);
		Node*fp = filetable_head;
		for(i=0;i<ptpp->file_table_size;i++){
			memcpy(&ptpp->file_table[i],fp,sizeof(Node));
			fp=fp->pNext;
		}
		pthread_mutex_unlock(file_table_mutex);
		printf("===========send file update=================\n");
		int res = sendPkt_c(ptp_conn, ptpp);
		if (res < 0) {
			perror("sending file-update failure");;
			return NULL;
		}
		needUpdate = 0;
		 printf("send ptp_peer_t type:%d\tpeer_ip:%s\tport:%d\tfile_table_size:%d\n",ptpp->type,
				ptpp->peer_ip,ptpp->port,ptpp->file_table_size);
	}
	free(ptpp);
}
