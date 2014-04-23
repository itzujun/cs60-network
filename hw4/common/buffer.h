#ifndef SRTCLIENT_H
#define SRTCLIENT_H
#include "../client/srt_client.h"



void clientBuf_append(client_tcb_t *tcb, segBuf_t* bufNode);
void serverBuf_append(svr_tcb_t *tcb, segBuf_t* bufNode);
#endif