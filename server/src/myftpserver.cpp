#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <vector>
#include <map>
#include <cstring>
#include "myftpserver.h"


using namespace std;
using namespace ftp;
# define PORT 12345

Server::Server(int port) {
	this->port = port;
	this->sd = -1;
	this->connQueue = 10;
	this->protocol = new unsigned char[6] { 0xe3, 'm', 'y', 'f', 't', 'p' };

	memset(&this->serverAddress, 0, sizeof(this->serverAddress));
	this->serverAddress.sin_family = AF_INET;
	this->serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	this->serverAddress.sin_port = htons(port);
}

void Server::start() {
	/*
	 * socket() -> bind() -> listen() -> accept() -> read() | write()
	 */
	//socket ipv4 protocol , stream socket (for TCP)
	this->sd = socket(AF_INET, SOCK_STREAM, 0);
	//bind: address:port
	if (bind(this->sd, (struct sockaddr *) &this->serverAddress,
			sizeof(this->serverAddress)) < 0) {
		printf("bind error: %s (Errno:%d)\n", strerror(errno), errno);
		exit(0);
	}

	//listen: set the port to be listening to incoming connections
	if (listen(this->sd, this->connQueue) < 0) {
		printf("listen error: %s (Errno:%d)\n", strerror(errno), errno);
		exit(0);
	}
}

void Server::loop() {
	while (1) {
		int client_sd;
		struct sockaddr_in client_addr;
		socklen_t addr_len = sizeof(client_addr);

		printf("BEFORE ACCEPT\n");
		if ((client_sd = accept(sd, (struct sockaddr *) &client_addr, &addr_len))
				< 0) {
			printf("accept erro: %s (Errno:%d)\n", strerror(errno), errno);
			exit(0);
		} else {

			printf("[FTP] new connection from %s:%d\n",
					inet_ntoa(client_addr.sin_addr), client_addr.sin_port);
			ConnectionHandler* handler = new ConnectionHandler(client_sd,
					client_addr);
			this->sockets.push_back(handler);
			handler->loop();
			return;
		}
		printf("AFTER ACCEPT\n");
	}
}


void ConnectionHandler::loop() {
	printf("test\n");

	struct message_s msg;
	//msg.protocol = new unsigned char[6] { 0xe3, 'm', 'y', 'f', 't', 'p' };
	//exit(0);
	while (1) {
		char buff[100];
		int len;
		printf("BEFORE RECV\n");
		if ((len = recv(sd, buff, sizeof(buff), 0)) <= 0) {
			//may be client shutdown or lost connection
			printf("receive error: %s (Errno:%d)\n", strerror(errno), errno);
			exit(0);
		}

		printf("AFTER RECV\n");
		buff[len] = '\0';
		printf("RECEIVED INFO: ");
		if (strlen(buff) != 0)
			printf("%s\n", buff);
		if (strcmp("exit", buff) == 0) {
			close(sd);
			return;
			break;
		}
	}
}


int main(int argc, char** argv) {

	if (argc != 2) {
		printf("invalid number of arguments %d", argc);
		exit(0);
	}
	Server server(atoi(argv[1]));
	server.start();

	printf("FTP Server started. protocol: %s \n", server.protocol);
	server.loop();
	printf("FTP Server shutdown.\n");
	return 0;
}

