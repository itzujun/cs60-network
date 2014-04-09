/*
 * File:   main.c
 * Author: starlight36
 *
 * Created on 2012年9月19日, 下午8:45
 */

#include <stdio.h>        // for printf
#include <stdlib.h>        // for exit
#include <string.h>        // for bzero
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>    // for sockaddr_in
#include <sys/types.h>    // for socket
#include <sys/socket.h>    // for socket
#include <sys/stat.h>
#include <pthread.h>

#define SERVER_PORT 47789
#define LISTEN_QUEUE_LENGTH 20
#define BUFFER_SIZE 1024
#define MAX_THREAD_NUM 5

#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

int thread_count = 0;
pthread_t threads[MAX_THREAD_NUM];

/**
 * 接收数据的执行函数
 * @param conn_socket
 */
int recv_data(int conn_socket);


/*
 * 主方法
 */
int main(int argc, char** argv)
{
	// 建立Socket
	int server_socket = socket(PF_INET, SOCK_STREAM, 0);
	if (server_socket < 0) {
		printf("Socket create failed.\n");
		return EXIT_FAILURE;
	}

	{
		int opt = 1;
		setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (opt));
	}

	// 设置服务器端监听套接字参数
	struct sockaddr_in server_addr;
	bzero(&server_addr, sizeof (server_addr));

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htons(INADDR_ANY);
	server_addr.sin_port = htons(SERVER_PORT);
	// 绑定端口
	if (bind(server_socket, (struct sockaddr*) &server_addr, sizeof (server_addr)))
	{
		printf("Bind port %d failed.\n", SERVER_PORT);
		return EXIT_FAILURE;
	}

	// 开始监听
	if (listen(server_socket, LISTEN_QUEUE_LENGTH))
	{
		 printf("Server Listen Failed!");
		 return EXIT_FAILURE;
	}

	// 开始处理客户端连接
	bzero(&threads, sizeof(pthread_t) * MAX_THREAD_NUM);
	while (1)
	{
		struct sockaddr_in client_addr;
		socklen_t length = sizeof(client_addr);

		// 建立客户端连接
		int client_conn = accept(server_socket, (struct sockaddr*) &client_addr, &length);
		if (client_conn < 0)
		{
			printf("Server Accept Failed!\n");
			return EXIT_FAILURE;
		}else{
			puts("client found!");
		}

		// 新建线程, 使用新线程与客户端交互
		int pthread_err = pthread_create(threads + (thread_count++), NULL, (void *)recv_data, (void *)client_conn);
		if (pthread_err != 0)
		{
			printf("Create thread Failed!\n");
			return EXIT_FAILURE;
		}

	}
	close(server_socket);
	return (EXIT_SUCCESS);
}

int recv_data(int conn_socket)
{
	char buffer[BUFFER_SIZE], msg[BUFFER_SIZE], num_str[88];
	char auth_passwd[] = "AUTH secretpassword\n";
	int port_num = 56152;
	bzero(buffer, BUFFER_SIZE);
	bzero(msg, BUFFER_SIZE);

//	int length = 0;
//	while (length = recv(conn_socket, buffer, BUFFER_SIZE, MSG_WAITALL))
//	{
//		if (length < 0)
//		{
//			printf("Server Recieve Data Failed! code %d\n", length);
//		}
//		printf("%s", buffer);
//		bzero(buffer, BUFFER_SIZE);
//
//	}

    //Receive a reply from the server
    //puts(server_reply);
    if( recv(conn_socket , buffer , BUFFER_SIZE , 0) < 0)
    {
        puts("server recv failed");
    }

    if(memcmp(auth_passwd, buffer, min(sizeof(auth_passwd), sizeof(buffer))) == 0){
    	sprintf(msg, "CONNECT threemileisland.cs.dartmouth.edu %d networks\n"
    			, port_num);;
        if( send(conn_socket , msg , strlen(msg) , 0) < 0)
        {
            puts("Send failed");
            return 1;
        }
        puts(msg);
    }

	close(conn_socket);

	return 0;
}
