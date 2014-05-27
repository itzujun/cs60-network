#include "pkt.h"
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
#define MAXLINE 200
//tracker side send & recv
int sendPkt(int connfd, ptp_tracker_t * pkt){
	char bufstart[2];
	char bufend[2];
	bufstart[0] = '!';
	bufstart[1] = '&';
	bufend[0] = '!';
	bufend[1] = '#';
	if(send(connfd, bufstart, 2, 0) < 0) {
		return -1;
	}
	if(send(connfd, pkt, sizeof(ptp_tracker_t),0)<0) {
		return -1;
	}
	if(send(connfd,bufend,2,0)<0) {
		return -1;
	}
	return 1;
}

int recvPkt(int connfd, ptp_peer_t * pkt){
	char buf[sizeof(ptp_peer_t)+2]; 
	char c;
	int idx = 0;
	// state can be 0,1,2,3; 
	// 0 starting point 
	// 1 '!' received
	// 2 '&' received, start receiving segment
	// 3 '!' received,
	// 4 '#' received, finish receiving segment 
	int state = 0; 
	while(recv(connfd,&c,1,0)>0) {
		if (state == 0) {
		        if(c=='!')
				state = 1;
		}
		else if(state == 1) {
			if(c=='&') 
				state = 2;
			else
				state = 0;
		}
		else if(state == 2) {
			if(c=='!') {
				buf[idx]=c;
				idx++;
				state = 3;
			}
			else {
				buf[idx]=c;
				idx++;
			}
		}
		else if(state == 3) {
			if(c=='#') {
				buf[idx]=c;
				idx++;
				memcpy(pkt,buf,sizeof(ptp_peer_t));
				state = 0;
				idx = 0;
				return 1;
			}
			else if(c=='!') {
				buf[idx]=c;
				idx++;
			}
			else {
				buf[idx]=c;
				idx++;
				state = 2;
			}
		}
	}
	return -1;
}






//peer side send & recv
int sendPkt_c(int connfd, ptp_peer_t * pkt){
    char bufstart[2];
    char bufend[2];
    bufstart[0] = '!';
    bufstart[1] = '&';
    bufend[0] = '!';
    bufend[1] = '#';
    if(send(connfd, bufstart, 2, 0) < 0) {
        return -1;
    }
    if(send(connfd, pkt, sizeof(ptp_peer_t),0)<0) {
        return -1;
    }
    if(send(connfd,bufend,2,0)<0) {
        return -1;
    }
    return 1;
}


int recvPkt_c(int connfd, ptp_tracker_t * pkt){
    char buf[sizeof(ptp_tracker_t)+2]; 
    char c;
    int idx = 0;
    // state can be 0,1,2,3; 
    // 0 starting point 
    // 1 '!' received
    // 2 '&' received, start receiving segment
    // 3 '!' received,
    // 4 '#' received, finish receiving segment 
    int state = 0; 
    while(recv(connfd,&c,1,0)>0) {
        if (state == 0) {
                if(c=='!')
                state = 1;
        }
        else if(state == 1) {
            if(c=='&') 
                state = 2;
            else
                state = 0;
        }
        else if(state == 2) {
            if(c=='!') {
                buf[idx]=c;
                idx++;
                state = 3;
            }
            else {
                buf[idx]=c;
                idx++;
            }
        }
        else if(state == 3) {
            if(c=='#') {
                buf[idx]=c;
                idx++;
                memcpy(pkt,buf,sizeof(ptp_tracker_t));
                state = 0;
                idx = 0;
                return 1;
            }
            else if(c=='!') {
                buf[idx]=c;
                idx++;
            }
            else {
                buf[idx]=c;
                idx++;
                state = 2;
            }
        }
    }
    return -1;
}

char*get_from_config(char* attr){
	FILE* file;
	char* entry = malloc(MAXLINE);;
	if((file =fopen("./common/ptp.config", "r")) == NULL){
		perror("fail to open ");
		return NULL;
	}
	while(fgets(entry,MAXLINE,file)!= NULL){
		entry[strlen(entry)-1]='\0';
		if(strstr(entry,attr)!=NULL){
			fclose(file);
			return strchr(entry,':')+1;
		}
	}
	fclose(file);
	return NULL;
}

int p2pSendPkt(int connfd, char * pkt, int len){
	char bufstart[2];
	char bufend[2];
	bufstart[0] = '!';
	bufstart[1] = '&';
	bufend[0] = '!';
	bufend[1] = '#';
	if(send(connfd, bufstart, 2, 0) < 0) {
		return -1;
	}
	if(send(connfd, pkt, len, 0)<0) {
		return -1;
	}
	if(send(connfd,bufend,2,0)<0) {
		return -1;
	}
	return 1;
}

int p2pRecvPkt(int connfd, char * pkt, int len){
	char buf[sizeof(ptp_peer_t)+2]; 
	char c;
	int idx = 0;
	// state can be 0,1,2,3; 
	// 0 starting point 
	// 1 '!' received
	// 2 '&' received, start receiving segment
	// 3 '!' received,
	// 4 '#' received, finish receiving segment 
	int state = 0; 
	while(recv(connfd,&c,1,0)>0) {
		if (state == 0) {
		        if(c=='!')
				state = 1;
		}
		else if(state == 1) {
			if(c=='&') 
				state = 2;
			else
				state = 0;
		}
		else if(state == 2) {
			if(c=='!') {
				buf[idx]=c;
				idx++;
				state = 3;
			}
			else {
				buf[idx]=c;
				idx++;
			}
		}
		else if(state == 3) {
			if(c=='#') {
				buf[idx]=c;
				idx++;
				memcpy(pkt,buf,len);
				state = 0;
				idx = 0;
				return 1;
			}
			else if(c=='!') {
				buf[idx]=c;
				idx++;
			}
			else {
				buf[idx]=c;
				idx++;
				state = 2;
			}
		}
	}
	return -1;
}
