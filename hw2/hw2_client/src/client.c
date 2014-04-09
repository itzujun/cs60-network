#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>

#define GET_AUTH 0
#define GET_CONN 1
#define OPERATION 2
#define CLOSE 3
#define BUFFER_SIZE 1024

static int welcomed = 0;

int create_socket() {
	int sock;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		printf("Could not create socket");
	}
	// puts("Socket created");

	return sock;
}

struct sockaddr_in config_server(int port) {
	struct sockaddr_in server;

	server.sin_addr.s_addr = inet_addr("129.170.213.101");
	//server.sin_addr.s_addr = inet_addr("127.0.0.1");
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	//server.sin_port = htons( 8888 );

	return server;
}

char* msg_for_operation(int selection) {
	//void *block = malloc(2000);
	//memset(block, 0, s);
	char* message;

	printf(
			"Which sensor would you like to read : \n \
\n \
\t(1) Water temperature\n \
\t(2) Reactor temperature\n \
\t(3) Power level\n\n\
Selection: ");
	scanf("%d", &selection);

	switch (selection) {
	case 1:
		message = "WATER TEMPERATURE";
		break;
	case 2:
		message = "REACTOR TEMPERATURE";
		break;
	case 3:
		message = "POWER LEVEL";
		break;
	default:
		message = "\0";
		break;
	}

	return message;
}

char* get_time_str(char server_reply[]) {
	time_t raw_time, current_time2;
	char* c_time_string, c_time_string2;
	char time_str[BUFFER_SIZE];

	/* Obtain current time as seconds elapsed since the Epoch. */
	memcpy(time_str, &server_reply[0], 12);
	time_str[10] = '\0';
	//time_str = "1346426869";
	raw_time = atol(time_str);

	if (raw_time == ((time_t) -1)) {
		(void) fprintf(stderr, "Failure to compute the current time.");
		return 0;
	}

	/* Convert to local time format. */
	c_time_string = ctime(&raw_time);
	//puts(c_time_string);

	if (c_time_string == NULL) {
		(void) fprintf(stderr, "Failure to convert the current time.");
		return 0;
	}

	/* Print to stdout. */
	c_time_string[strlen(c_time_string) - 1] = '\0';
	return c_time_string;
}

char* get_value_string(char server_reply[]) {
	char value_string[100];
	memcpy(value_string, &server_reply[13], strlen(server_reply) - 12);
	value_string[strlen(server_reply) - 13] = '\0';
	return value_string;
}

char* comm_with_server(int sock, struct sockaddr_in server, int phase) {
	char server_reply[BUFFER_SIZE], return_msg[BUFFER_SIZE], msg[BUFFER_SIZE];
	char* message;
	static int selection;
	memset(server_reply, 0, BUFFER_SIZE);
	memset(return_msg, 0, BUFFER_SIZE);
	memset(msg, 0, BUFFER_SIZE);

	while (1) {
		if (phase == GET_AUTH) {
			message = "AUTH secretpassword";
		} else if (phase == GET_CONN) {
			message = "AUTH networks";
		} else if (phase == OPERATION) {
			message = msg_for_operation(selection);
		} else if (phase == CLOSE) {
			message = "CLOSE";
		}
		//printf("Enter message : ");
		//scanf(" %[^\n]s", message);

		if (message[0] != 0) {
			//Send some data
			strcat(msg, message);
			strcat(msg, "\n");
			if (send(sock, msg, strlen(msg), 0) < 0) {
				puts("Send failed");
				return 1;
			}

			//Receive a reply from the server
			//puts(server_reply);
			if (recv(sock, server_reply, BUFFER_SIZE, 0) < 0) {
				puts("recv failed");
				break;
			}
		}

		//puts("Server reply :");
		//puts(server_reply);
		if (phase == GET_AUTH) {
			memcpy(return_msg, &server_reply[41], 5);
			return_msg[5] = '\0';
		} else if (phase == OPERATION) {
			if (message[0] == 0) {
				strcat(return_msg, "\n");
				strcat(return_msg, "*** Invalid selection.");
				strcat(return_msg, "\n");
			} else {
				strcat(return_msg, "\n\t");
				strcat(return_msg, "The last ");
				strcat(return_msg, message);
				strcat(return_msg, " was taken ");
				strcat(return_msg, get_time_str(server_reply));
				strcat(return_msg, " and was ");
				strcat(return_msg, get_value_string(server_reply));
				strcat(return_msg, "\n");
			}
		} else {
			memcpy(return_msg, &server_reply[0], strlen(server_reply));
			return_msg[strlen(server_reply)] = '\0';
		}
		return return_msg;
	}
}

int talk_to_3mile(int port) {
	int sock;
	char* recv_str;
	struct sockaddr_in server;

	// Create socket
	sock = create_socket();

	// Configure server info
	if (port == -1)
		server = config_server(47789);
	else
		server = config_server(port);

	// Connect to remote server
	if (connect(sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
		perror("connect failed. Error");
		return 1;
	}

	if (port == -1) {
		// Get auth port
		recv_str = comm_with_server(sock, server, GET_AUTH);
		if (!welcomed) {
			puts("WELCOME TO THE THREE MILE ISLAND SENSOR NETWORK\n\n");
			welcomed = 1;
		}
	} else {
		// hand shake
		recv_str = comm_with_server(sock, server, GET_CONN);
		// operation
		recv_str = comm_with_server(sock, server, OPERATION);
		puts(recv_str);
		// say bye
		recv_str = comm_with_server(sock, server, CLOSE);
	}
	close(sock);
	return atoi(recv_str);
}

int main() {
	int port;

	while (1) {
		port = talk_to_3mile(-1);
		talk_to_3mile(port);
	}

	return 0;
}

