#include"buffer.h"

void clientBuf_append(client_tcb_t *tcb, segBuf_t* bufNode) {
	if(tcb == NULL)
		printf("err in %s: tcb entry is NUll!\n", __func__);
	if(bufNode == NULL)
		printf("err in %s: bufNode is NUll!\n", __func__);

	bufNode->next = NULL;
	if(tcb->sendBufTail == NULL){
		// the buffer is currently empty
		tcb->sendBufTail = sendBufHead = bufNode;
	} else {
		tcb->sendBufTail->next = bufNode;
		tcb->sendBufTail = sendBufTail->next;
	}
}

void serverBuf_append(svr_tcb_t *tcb, segBuf_t* bufNode) {
	if(tcb == NULL)
		printf("err in %s: tcb entry is NUll!\n", __func__);
	if(bufNode == NULL)
		printf("err in %s: bufNode is NUll!\n", __func__);

	bufNode->next = NULL;
	if(tcb->sendBufTail == NULL){
		// the buffer is currently empty
		tcb->sendBufTail = sendBufHead = bufNode;
	} else {
		tcb->sendBufTail->next = bufNode;
		tcb->sendBufTail = sendBufTail->next;
	}
}

void buffer_findAndRm(segBuf_t* sendBufTail, segBuf_t* sendBufHead, int ack_num) {

}

