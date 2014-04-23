#ifndef SRTCLIENT_H
#define SRTCLIENT_H
#include "../client/srt_client.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

void clientBuf_append(client_tcb_t *tcb, segBuf_t* bufNode);
void serverBuf_append(svr_tcb_t *tcb, segBuf_t* bufNode);
#endif