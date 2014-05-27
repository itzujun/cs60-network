#ifndef CONSTANTS_H
#define CONSTANTS_H

#define HANDSHAKE_PORT 12134
#define BUF_SIZE 500
#define PROTOCOL_LEN 100
#define RESERVED_LEN 100
#define IP_LEN 20
#define LISTENING_PORT 14332
#define MAX_PEER_NUM 20
#define MAX_FILE_NUM 100
#define ALIVE_POLLING_TIME 60
#define ALIVE_TIMEOUT 600
#define FILE_NAME_MAX_LEN 127
#define INTERVAL 120
#define PIECE_LENGTH 1024
#define MD5_LEN 33
#define MAX_MONITOR_DIR 100
#define FILE_UPDATE_INTERVAL 1

// added by gjj
#define P2P_LISTENING_PORT 3101
#define P2P_DOWNLOAD_PORT 4100
#define P2P_DOWNLOAD_PORT_MAX 5100
#define PIECE_SIZE 1024 // as 1 kB
#define OPEN_FILE_TIMEOUT 1
#define WAIT_PEER_INTERVAL 0
#define WAIT_PIECE_INTERVAL 1
#define WAIT_THREAD_INTERVAL 1
#define WAIT_NET_UP_INTERVAL 0
#define WAIT_FOR_THREAD_START 3
#define MAX_THREAD_NUM 99
#endif
