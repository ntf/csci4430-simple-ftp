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
#include <fstream>
#include <string>
#include <map>
#include "myftpserver.h"

using namespace std;
using namespace ftp;
# define PORT 12345

Server* instance;
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

void Server::log(const char* str) {
	printf("[FTP]%s\n", str);
}
void Server::start() {

	{
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

	//access control
	string line;
	ifstream myfile("access.txt");
	if (myfile.is_open()) {
		int i = 0;
		string username;
		string password;
		this->log("import usernames:");
		while (std::getline(myfile, line)) {
			std::vector<string> v = explode(line, ' ');
			this->credentials[v[0]] = v[1];
			this->log(v[0].c_str());

		}
		myfile.close();
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
			printf("handler end\n");
			//	return;
		}
		printf("AFTER ACCEPT\n");
	}
}

void ConnectionHandler::loop() {
	printf("test\n");
	struct message_s* res = new struct message_s;
	memcpy(res->protocol, this->protocol, 6);
	struct message_s* req = NULL;
	while (1) {
		try {
			//printf("BEFORE read header\n");
			struct message_s* req = this->readHeader(Protocols::ANY);
			this->log("new command\n");
			char* payload = NULL;
			if (req->length > 12) {
				payload = this->readPayload(
						req->length - sizeof(struct message_s));
				//this->log("read payload");
			}
			//printf("after read header\n");

			switch (req->type) {
			case Protocols::OPEN_CONN_REQUEST:
				if (this->state == ConnectionHandler::INITIAL) {
					res->type = Protocols::OPEN_CONN_REPLY;
					res->length = 12;
					res->status = 1;
					this->emit(res, sizeof(struct message_s));
					this->state = ConnectionHandler::CONNECTED;
				}
				break;
			case Protocols::AUTH_REQUEST:
				if (this->state == ConnectionHandler::CONNECTED) {
					std::vector<string> tokens = explode(string(payload), ' ');
					//std::map<std::string, std::string> credentials;
					res->type = Protocols::AUTH_REPLY;
					res->length = 12;
					res->status = 0;
					/*
					 for (std::vector<int>::size_type i = 0; i != tokens.size(); i++) {
					 cout << tokens[i] << "\n";
					 }*/
					if (instance->credentials.count(tokens[0]) == 1) {
						if (instance->credentials[tokens[0]] == tokens[1]) {
							res->status = 1;
							this->state = ConnectionHandler::AUTHED;
						}
					}
					this->emit(res, sizeof(struct message_s));
				}
				break;
			case Protocols::LIST_REQUEST:
				break;
			case Protocols::GET_REQUEST:
				break;
			case Protocols::PUT_REQUEST:
				break;
			case Protocols::QUIT_REQUEST:
				res->type = Protocols::QUIT_REPLY;
				res->length = 12;
				res->status = 1;
				printf("[FTP]%s:%d disconnected\n",
						inet_ntoa(this->addr.sin_addr), this->addr.sin_port);
				this->emit(res, sizeof(struct message_s));
				this->state = ConnectionHandler::INITIAL;
				close(this->sd);
				return;
				break;
			}

		} catch (int ex) {
			switch (ex) {
			case Exceptions::PROTOCOL_NOT_MATCHED:
				this->log("[ERROR] unknown protocol\n");
				break;
			case Exceptions::SOCKET_RECV_ERROR:
			case Exceptions::SOCKET_SEND_ERROR:
			case Exceptions::SOCKET_DISCONNECTED:
				this->log("disconnected");
				close(this->sd);
				return;
			}
		}

		if (req != NULL) {
			free(req);
			req = NULL;
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
	instance = &server;

	printf("FTP Server started. protocol: %s \n", server.protocol);
	server.loop();
	printf("FTP Server shutdown.\n");
	return 0;
}

