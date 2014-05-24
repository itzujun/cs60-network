#ifndef PEER_H
#define PEER_H

#include "../common/constants.h"
#include "../common/pkt.h"
#include "../inotify/inotify_watch.h"

/*
init peer table, return failed? -1:1
*/
void* ptp_listening(void*arg);
void* file_monitor(void*arg);
void* keep_alive(void*arg);
void sendFileUpdate();
void* ptp_transfer(void*arg);
void* upload(void*arg);


/*
init file table, return failed? -1:1
*/
#endif
