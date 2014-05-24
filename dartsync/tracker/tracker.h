#ifndef TRACKER_H
#define TRACKER_H
#include "../common/constants.h"

#include "../common/pkt.h"
/*
init peer table, return failed? -1:1
*/
void initPeerTable();

/*
init file table, return failed? -1:1
*/
void initFileTable();

void updateFileTable(ptp_peer_t * pkt);

void broadcastFileTable();

#endif
