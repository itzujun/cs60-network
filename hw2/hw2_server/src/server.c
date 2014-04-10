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
char auth_passwd[] = "AUTH secretpassword\n";
char auth_network[] = "AUTH networks\n";
char water[] = "WATER TEMPERATURE\n";
char reactor[] = "REACTOR TEMPERATURE\n";
char power[] = "POWER LEVEL\n";
char auth_close[] = "CLOSE\n";
pthread_t threads[MAX_THREAD_NUM];
static int port_num = 49153;

/**
 * the thread function to handle multiple connections
 * @param conn_socket
 */
int handle_clients(int conn_socket);
int server_socket_setup(int port);

/*
 * main
 */
int main(int argc, char** argv) {
	srand(time(NULL));
	int server_socket = server_socket_setup(SERVER_PORT);

	// handling new coming request
	bzero(&threads, sizeof(pthread_t) * MAX_THREAD_NUM);
	while (1) {
		struct sockaddr_in client_addr;
		socklen_t length = sizeof(client_addr);

		// connect with client
		puts("start listening...");
		int client_conn = accept(server_socket, (struct sockaddr*) &client_addr,
				&length);
		if (client_conn < 0) {
			printf("Server Accept Failed!\n");
			return EXIT_FAILURE;
		}

		// creating new thread
		int pthread_err = pthread_create(threads + (thread_count++), NULL,
				(void *) handle_clients, (void *) client_conn);
		if (pthread_err != 0) {
			printf("Create thread Failed!\n");
			return EXIT_FAILURE;
		}

	}
	close(server_socket);
	return (EXIT_SUCCESS);
}

int server_socket_setup(int port) {
	// build server Socket
	int server_socket = socket(PF_INET, SOCK_STREAM, 0);
	if (server_socket < 0) {
		printf("Socket create failed.\n");
		return EXIT_FAILURE;
	} else {
		int opt = 1;
		setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	}

	// set server configuration
	struct sockaddr_in server_addr;
	bzero(&server_addr, sizeof(server_addr));

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htons(INADDR_ANY);
	server_addr.sin_port = htons(port);

	// bind the port
	if (bind(server_socket, (struct sockaddr*) &server_addr,
			sizeof(server_addr))) {
		printf("Bind port %d failed.\n", port);
		return -1;
	}

	// listening
	if (listen(server_socket, LISTEN_QUEUE_LENGTH)) {
		printf("Server Listen Failed!");
		return EXIT_FAILURE;
	}

	return server_socket;
}

int str_compare(char buff1[], char buff2[]) {
	return memcmp(buff1, buff2, min(sizeof(buff1), sizeof(buff2)));
}

int tell_port_num(int conn_socket) {
	char buffer[BUFFER_SIZE], msg[BUFFER_SIZE], num_str[88];
	int  server_socket = -1;
	bzero(buffer, BUFFER_SIZE);
	bzero(msg, BUFFER_SIZE);

	if (recv(conn_socket, buffer, BUFFER_SIZE, 0) < 0) {
		puts("tell_port_num: server recv failed");
	}

	if (str_compare(auth_passwd, buffer) == 0) {
		// build a server_server socket
		while (server_socket == -1) {
			port_num = port_num < 65535 ? port_num + 1 : 49153;
			server_socket = server_socket_setup(port_num);
		}

		// tell client the new socket
		sprintf(msg, "CONNECT threemileisland.cs.dartmouth.edu %d networks\n",
				port_num);
		;
		if (send(conn_socket, msg, strlen(msg), 0) < 0) {
			puts("Telling client fail");
			return -1;
		}
		puts(msg);
	}
	close(conn_socket);
	return server_socket;
}

int handle_clients(int conn_socket) {
	char buffer[BUFFER_SIZE], msg[BUFFER_SIZE];

	// Tell client the operation port number
	int server_socket = tell_port_num(conn_socket);

	// handling new coming request
	bzero(&threads, sizeof(pthread_t) * MAX_THREAD_NUM);
	int client_conn = -1;
	while (1) {
		bzero(buffer, BUFFER_SIZE);
		bzero(msg, BUFFER_SIZE);

		// connect with client
		if (client_conn == -1) {
			struct sockaddr_in client_addr;
			socklen_t length = sizeof(client_addr);
			client_conn = accept(server_socket, (struct sockaddr*) &client_addr,
					&length);
			if (client_conn < 0) {
				printf("Server Accept Failed in child!\n");
				return EXIT_FAILURE;
			}
		}

		if (recv(client_conn, buffer, BUFFER_SIZE, 0) < 0) {
			puts("handle_clients: server recv failed");
			sleep(100);
			continue;
		}

		if (str_compare(auth_network, buffer) == 0) {
			// tell client the new socket
			sprintf(msg, "SUCCESS\n");
			if (send(client_conn, msg, strlen(msg), 0) < 0) {
				puts("Telling client fail");
				return -1;
			}
		} else if (str_compare(water, buffer) == 0) {
			puts("in the water");
			int temp = rand() % 100;
			sprintf(msg, "%d %d F\n", (int)time(NULL), temp);
			if (send(client_conn, msg, strlen(msg), 0) < 0) {
				puts("Telling client fail");
				return -1;
			}
			puts(msg);
		} else if (str_compare(reactor, buffer) == 0) {
			int temp = rand() % 100;
			sprintf(msg, "%d %d F\n", (int)time(NULL), temp);
			if (send(client_conn, msg, strlen(msg), 0) < 0) {
				puts("Telling client fail");
				return -1;
			}
		} else if (str_compare(power, buffer) == 0) {
			int temp = rand() % 100;
			sprintf(msg, "%d %d Mw\n", (int)time(NULL), temp);
			if (send(client_conn, msg, strlen(msg), 0) < 0) {
				puts("Telling client fail");
				return -1;
			}
		} else if (str_compare(auth_close, buffer) == 0) {
			sprintf(msg, "BYE\n");
			if (send(client_conn, msg, strlen(msg), 0) < 0) {
				puts("Telling client fail");
				return -1;
			}
			close(server_socket);
			// Tell client the operation port number
			int server_socket = tell_port_num(conn_socket);
		}
	}
	close(server_socket);
	pthread_exit(NULL);
}
