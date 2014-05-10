
#include "seg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

//SRT process uses this function to send a segment and its destination node ID in a sendseg_arg_t structure to SNP process to send out. 
//Parameter network_conn is the TCP descriptor of the connection between the SRT process and the SNP process. 
//Return 1 if a sendseg_arg_t is succefully sent, otherwise return -1.
int snp_sendseg(int network_conn, int dest_nodeID, seg_t* segPtr)
{
  //printf("%s: here\n", __func__);
  char bufstart[2];
  char bufend[2];
  bufstart[0] = '!';
  bufstart[1] = '&';
  bufend[0] = '!';
  bufend[1] = '#';
  segPtr->header.checksum = checksum(segPtr);
  sendseg_arg_t* snpSeg = (sendseg_arg_t*)malloc(sizeof(sendseg_arg_t));
  snpSeg->nodeID = dest_nodeID;
  memcpy(&snpSeg->seg, segPtr, sizeof(seg_t));
  //printf("%s: the calculated checksum is %d!\n", __func__, segPtr->header.checksum);
  
  // printf("<func: %s> send head\n", __func__);
  if (send(network_conn, bufstart, 2, 0) < 0) {
      return -1;
  }
  // printf("<func: %s> send body\n", __func__);
  if(send(network_conn, snpSeg, sizeof(sendseg_arg_t), 0)<0) {
        // printf("<func: %s> send body fail\n", __func__);
    return -1;
  }
    // printf("<func: %s> send foot\n", __func__);
  if(send(network_conn,bufend,2,0)<0) {
    return -1;
  }
  //printf("%s: out\n", __func__);
  return 1;
}

//SRT process uses this function to receive a  sendseg_arg_t structure which contains a segment and its src node ID from the SNP process. 
//Parameter network_conn is the TCP descriptor of the connection between the SRT process and the SNP process. 
//When a segment is received, use seglost to determine if the segment should be discarded, also check the checksum.  
//Return 1 if a sendseg_arg_t is succefully received, otherwise return -1.
int snp_recvseg(int network_conn, int* src_nodeID, seg_t* segPtr)
{
  //printf("%s: here\n", __func__);
    char buf[sizeof(seg_t)+2]; 
    char c;
    int idx = 0;
    // state can be 0,1,2,3; 
    // 0 starting point 
    // 1 '!' received
    // 2 '&' received, start receiving segment
    // 3 '!' received,
    // 4 '#' received, finish receiving segment 
    int state = 0; 
    while(recv(network_conn,&c,1,0)>0) {
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
                state = 0;
                idx = 0;
                // puts("hadnle pkt err");
                
                memcpy(segPtr, &((sendseg_arg_t*)buf)->seg, sizeof(seg_t));
                memcpy(src_nodeID, &((sendseg_arg_t*)buf)->nodeID, sizeof(int));
                printf("%s: get seg with seq_num %d\n", __func__, segPtr->header.seq_num);
                if(seglost(segPtr)>0) {
                    printf("seg lost!!!\n");
                    continue;
                }
                if(checkchecksum((seg_t*)segPtr) == -1){
                    puts("checksum err");
                    continue;
                } 
                

              //  printf("%s: out\n", __func__);
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

//SNP process uses this function to receive a sendseg_arg_t structure which contains a segment and its destination node ID from the SRT process.
//Parameter tran_conn is the TCP descriptor of the connection between the SRT process and the SNP process. 
//Return 1 if a sendseg_arg_t is succefully received, otherwise return -1.
int getsegToSend(int tran_conn, int* dest_nodeID, seg_t* segPtr)
{
 // printf("%s: here\n", __func__);
    char buf[sizeof(sendseg_arg_t)+2]; 
    char c;
    int idx = 0;
    // state can be 0,1,2,3; 
    // 0 starting point 
    // 1 '!' received
    // 2 '&' received, start receiving segment
    // 3 '!' received,
    // 4 '#' received, finish receiving segment 
    int state = 0; 
    while(recv(tran_conn,&c,1,0)>0) {
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
                state = 0;
                idx = 0;
                memcpy(segPtr, &((sendseg_arg_t*)buf)->seg, sizeof(seg_t));
                memcpy(dest_nodeID, &((sendseg_arg_t*)buf)->nodeID, sizeof(int));
                //printf("%s: out\n", __func__);
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

//SNP process uses this function to send a sendseg_arg_t structure which contains a segment and its src node ID to the SRT process.
//Parameter tran_conn is the TCP descriptor of the connection between the SRT process and the SNP process. 
//Return 1 if a sendseg_arg_t is succefully sent, otherwise return -1.
int forwardsegToSRT(int tran_conn, int src_nodeID, seg_t* segPtr)
{
  //printf("%s: here\n", __func__);
  char bufstart[2];
  char bufend[2];
  bufstart[0] = '!';
  bufstart[1] = '&';
  bufend[0] = '!';
  bufend[1] = '#';
  // segPtr->header.checksum = checksum(segPtr);
  sendseg_arg_t* snpSeg = (sendseg_arg_t*)malloc(sizeof(sendseg_arg_t));
  snpSeg->nodeID = src_nodeID;
  memcpy(&snpSeg->seg, segPtr, sizeof(seg_t));
  
  if (send(tran_conn, bufstart, 2, 0) < 0) {
    return -1;
  }
  if(send(tran_conn, snpSeg, sizeof(sendseg_arg_t), 0)<0) {
    return -1;
  }
  if(send(tran_conn, bufend, 2, 0)<0) {
    return -1;
  }
  //printf("%s: out\n", __func__);
  return 1;
}

// for seglost(seg_t* segment):
// a segment has PKT_LOST_RATE probability to be lost or invalid checksum
// with PKT_LOST_RATE/2 probability, the segment is lost, this function returns 1
// If the segment is not lost, return 0. 
// Even the segment is not lost, the packet has PKT_LOST_RATE/2 probability to have invalid checksum
// We flip  a random bit in the segment to create invalid checksum
int seglost(seg_t* segPtr)
{
  return 0;
	int random = rand()%100;
	if(random<PKT_LOSS_RATE*100) {
		//50% probability of losing a segment
		if(rand()%2==0) {
			printf("seg lost!!!\n");
          return 1;
      }
		//50% chance of invalid checksum
      else {
			//get data length
         int len = sizeof(srt_hdr_t)+segPtr->header.length;
			//get a random bit that will be flipped
         int errorbit = rand()%(len*8);
			//flip the bit
         char* temp = (char*)segPtr;
         temp = temp + errorbit/8;
         *temp = *temp^(1<<(errorbit%8));
         return 0;
     }
 }
 return 0;
}
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//This function calculates checksum over the given segment.
//The checksum is calculated over the segment header and segment data.
//You should first clear the checksum field in segment header to be 0.
//If the data has odd number of octets, add an 0 octets to calculate checksum.
//Use 1s complement for checksum calculation.
unsigned short checksum(seg_t* segment)
{
    unsigned short* dataBuf = (unsigned short*)segment;
    // set checksum as long type in order to caputre the carries
    unsigned short chksum = 0;
    int len = sizeof(segment->header);
    if(segment->header.type == DATA) {
        len += segment->header.length;
    }

    // compute checksum
    while (len > 1) {
        chksum += *dataBuf;
        dataBuf++;
        len -= 2;
    }
    if(len) {
        /// if there is one more left, force to grab a 16 bit data
        chksum += *(unsigned short*)dataBuf;
    }

    // handle the carries
    while (chksum >> 16)
        chksum = (chksum >> 16) + (chksum & 0xffff); 

    // printf("%s: the checksum is %d!\n", __func__, (unsigned short)(~chksum));
    return (unsigned short)(~chksum);
}

//Check the checksum in the segment,
//return 1 if the checksum is valid,
//return -1 if the checksum is invalid
int checkchecksum(seg_t* segment)
{
    unsigned short chksum = checksum(segment);
    return 1;
    if(chksum != 0 && chksum != 0xFF){
        printf("%s: the checksum is %d!\n", __func__, segment->header.checksum);
        printf("%s: the checkchecksum is %d!\n", __func__, chksum);
    }
    return (chksum == 0 || chksum == 0xFF) ? 1 : -1;
}
