#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>
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

void connection_terminate_thread(void* connection) {
	struct ConnectionThread* conn = ((struct ConnectionThread*) connection);
	cout << "(" << conn->thread << ") terminated \n";
	conn->socket->terminate();
}
/**
 * helper function for new thread
 */
void* connection_new_thread(void* connection) {
	pthread_cleanup_push(connection_terminate_thread,connection);
		struct ConnectionThread* conn = ((struct ConnectionThread*) connection);
		//cout << "new thread started\n";
		cout << "new thread started (" << conn->thread << ")\n";
		conn->socket->loop();

		pthread_cleanup_pop(1);
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

			struct ConnectionThread* connection = new struct ConnectionThread;
			connection->socket = new ConnectionHandler(client_sd, client_addr);
			connection->socket->setServer(this);
			//	pthread_create(&connection->thread, NULL,connection_new_thread, NULL);
			pthread_attr_t attr;
			pthread_attr_init(&attr);
			pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
			pthread_create(&connection->thread, &attr, connection_new_thread,
					(void *) connection);
			this->sockets.push_back(connection);

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
			this->log(payload);
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
				if (this->state == ConnectionHandler::AUTHED) {
					DIR *dir;
					string data;
					struct dirent *ent;
					if ((dir = opendir("filedir/")) != NULL) {
						/* print all the files and directories within directory */
						while ((ent = readdir(dir)) != NULL) {
							string fileName = string(ent->d_name);
							if (fileName != "." && fileName != "..") {
								data += fileName + "\n";
								printf("%s\n", ent->d_name);
							}
						}
						closedir(dir);
					} else {
						/* could not open directory */
						perror("");
						throw Exceptions::IO_ERROR;
					}

					res->type = Protocols::LIST_REPLY;
					res->length = 12 + data.size() + 1;
					res->status = 0;
					this->emit(res, sizeof(struct message_s));
					this->emit(data.c_str(), data.size() + 1);
				}
				break;
			case Protocols::GET_REQUEST:
				if (this->state == ConnectionHandler::AUTHED) {
					char* repository, *resolved_path;
					repository = realpath("filedir/", NULL);
					resolved_path =
							realpath(
									string(string("filedir/") + string(payload)).c_str(),
									NULL);
					cout << repository << " " << resolved_path << "\n";

					res->type = Protocols::GET_REPLY;
					res->length = 12;
					if (strncmp(resolved_path, repository, strlen(repository))
							== 0) {
						res->status = 1;
					} else {
						res->status = 0;
					}
					this->emit(res, sizeof(struct message_s));

					if (res->status == 1) {
						res->type = Protocols::FILE_DATA;
						struct stat st;
						stat(resolved_path, &st);
						res->length = 12 + st.st_size;
						res->status = 1;
						this->emit(res, sizeof(struct message_s));
						ifstream inFile(resolved_path, ios::binary | ios::in);
						do {
							inFile.read((char*) this->buff, sizeof(buff));
							if (inFile.gcount() > 0) {
								this->emit(this->buff, inFile.gcount());
							}
							cout << inFile.gcount() << "\n";
						} while (inFile.gcount() > 0);

						inFile.close();
					}
					//this->emit(data.c_str(), data.size());
				}
				break;
			case Protocols::PUT_REQUEST:

				if (this->state == ConnectionHandler::AUTHED) {
					string putFile = string(realpath(string("filedir/").c_str(),
					NULL)) + "/" + string(payload);

					res->type = Protocols::PUT_REPLY;
					res->length = 12;
					res->status = 1;
					this->emit(res, sizeof(struct message_s));

					struct message_s* file_data = this->readHeader(
							Protocols::FILE_DATA);
					ofstream outFile(putFile.c_str());
					if (file_data->length > 12) {
						char* data = this->readPayload(
								file_data->length - sizeof(struct message_s));
						outFile.write(data, file_data->length - 12);
					} else {
						outFile.write(NULL, 0);
					}

					cout << "write " << file_data->length - 12 << "bytes to "
							<< putFile << "\n";
					outFile.close();
				}

				break;
			case Protocols::QUIT_REQUEST:
				res->type = Protocols::QUIT_REPLY;
				res->length = 12;
				res->status = 1;
				printf("[FTP]%s:%d disconnected\n",
						inet_ntoa(this->addr.sin_addr), this->addr.sin_port);
				this->emit(res, sizeof(struct message_s));
				this->state = ConnectionHandler::INITIAL;
				this->terminate();
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
				this->terminate();
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

