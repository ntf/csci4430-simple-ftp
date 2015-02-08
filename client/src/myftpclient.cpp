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
#include <iostream>
#include <vector>
#include <fstream>
#include <cstring>
#include <string>
#include <iterator>
#include <map>
#include "myftpclient.h"

using namespace std;
using namespace ftp;
#define IPADDR "127.0.0.1"
#define PORT 12345

Client::Client() :
		ConnectionHandler() {

}
void Client::start() {
	//initial stuff
	this->loop();
}
void Client::setSD(int sd) {
	this->sd = sd;
}
void Client::setState(int state) {
	this->state = state;
}
int Client::getState() {
	return this->state;
}
//protocols
void Client::OPEN_CONN_REQUEST(struct message_s* req,
		std::vector<string> tokens) {
	int sd = socket(AF_INET, SOCK_STREAM, 0);
	this->sd = sd;
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(tokens[1].c_str());
	server_addr.sin_port = htons(atoi(tokens[2].c_str()));
	if (connect(sd, (struct sockaddr *) &server_addr, sizeof(server_addr))
			< 0) {
		printf("connection error: %s (Errno:%d)\n", strerror(errno),
		errno);
	}

	//Protocols::OPEN_CONN_REQUEST
	req->type = Protocols::OPEN_CONN_REQUEST;
	req->length = 12;
	req->status = 0;
	this->emit(req, sizeof(struct message_s));

	//Protocols::OPEN_CONN_REPLY
	struct message_s* res = this->readHeader(Protocols::OPEN_CONN_REPLY);
	if (res->status == 1) {
		cout << "Server connection accepted.\n";
		this->setState(Client::CONNECTED);
	} else {
		this->setState(Client::INITIAL);
		cout << "Server connection failed.\n";
	}

}
void Client::AUTH_REQUEST(struct message_s* req, std::vector<string> tokens) {
	//Protocols::OPEN_CONN_REQUEST
	req->type = Protocols::AUTH_REQUEST;
	const char* payload = (tokens[1] + " " + tokens[2]).c_str();
	req->length = 12 + strlen(payload) + 1;
	req->status = 0;
	this->emit(req, sizeof(struct message_s));

	this->emit(payload, strlen(payload) + 1);
	//Protocols::AUTH_REPLY
	struct message_s* res = this->readHeader(Protocols::AUTH_REPLY);
	if (res->status == 1) {
		cout << "Authentication granted.\n";
		this->username = tokens[1].size();
		this->setState(Client::AUTHED);
	} else {
		this->setState(Client::INITIAL);
		cout << "ERROR: Authentication rejected. Connection closed.\n";
		close(this->sd);
	}

}
void Client::LIST_REQUEST(struct message_s* req, std::vector<string> tokens) {
	req->type = Protocols::LIST_REQUEST;
	req->length = 12;
	req->status = 0;
	this->emit(req, sizeof(struct message_s));
	struct message_s* res = this->readHeader(Protocols::LIST_REPLY);
	char* payload = NULL;
	if (res->length > 12) {
		payload = this->readPayload(res->length - sizeof(struct message_s));
	}

	cout << "----- file list start -----\n";
	if (payload != NULL) {
		cout << payload;
	}
	cout << "----- file list end -----\n";

}
void Client::GET_REQUEST(struct message_s* req, std::vector<string> tokens) {
	req->type = Protocols::GET_REQUEST;
	const char* payload = tokens[1].c_str();
	req->length = 12 + strlen(payload) + 1;
	req->status = 0;
	cout << req->length << "\n";
	this->emit(req, sizeof(struct message_s));
	this->emit(payload, strlen(payload) + 1);

	struct message_s* res = this->readHeader(Protocols::GET_REPLY);
	if (res->status == 1) {
		struct message_s* fileHeader = this->readHeader(Protocols::FILE_DATA);
		ofstream outFile(tokens[1].c_str());
		if (fileHeader->length > 12) {
			int remains = fileHeader->length - 12;
			int total = remains;
			int current = 0;
			int percent = 0;
			while (remains > 0) {
				int size = this->receive(this->buff,
						remains < sizeof(this->buff) ?
								remains : sizeof(this->buff));
				outFile.write(this->buff, size);
				remains -= size;
				current = total - remains;
				if ((int) (((float) current / total) * 100.0) > percent) {
					cout << current << "/" << total << "(" << percent << "%)\n";
				}
				percent = ((float) current / total) * 100.0;

			}
		} else {
			outFile.write(NULL, 0);
		}

		outFile.close();
		cout << "write " << fileHeader->length - 12 << "bytes to " << tokens[1]
				<< "\n";

	} else {
		cout << "GET_REPLY:0.\n";
	}

}
void Client::PUT_REQUEST(struct message_s* req, std::vector<string> tokens) {
	const char* payload = tokens[1].c_str();
	char* repository, *resolved_path;
	repository = realpath("./", NULL);
	resolved_path = realpath(string(string("./") + string(payload)).c_str(),
	NULL);
	if (resolved_path == NULL || repository == NULL) {
		this->log("file not reachable");
		return;
	}
	//cout << repository << " " << resolved_path << "\n";
	//check if the target file inside current working directory
	if (strncmp(resolved_path, repository, strlen(repository)) != 0) {
		this->log("outside of current working directory");
		return;
	}

	ifstream inFile(resolved_path, ios::binary | ios::in);

	//check if input file really exists and accessible
	if (!inFile.good()) {
		this->log("failed to open the file or file doesn't exists.");
		return;
	}

	req->type = Protocols::PUT_REQUEST;
	req->length = 12 + strlen(payload) + 1;
	req->status = 0;
	this->emit(req, sizeof(struct message_s));

	this->emit(payload, strlen(payload) + 1);

	struct message_s* res = this->readHeader(Protocols::PUT_REPLY);

	req->type = Protocols::FILE_DATA;
	struct stat st;
	stat(resolved_path, &st);
	req->length = 12 + st.st_size;
	req->status = 1;
	this->emit(req, sizeof(struct message_s));

	int total = st.st_size;
	int current = 0;
	int percent = 0;
	do {
		inFile.read((char*) this->buff, sizeof(buff));
		if (inFile.gcount() > 0) {
			this->emit(this->buff, inFile.gcount());
		}
		current += inFile.gcount();


		if ((int) (((float) current / total) * 100.0) > percent) {
			cout << current << "/" << total << "(" << percent << "%)\n";
		}
		percent = ((float) current / total) * 100.0;
	} while (inFile.gcount() > 0);
	inFile.close();

}
void Client::QUIT_REQUEST(struct message_s* req, std::vector<string> tokens) {
//quit
	req->type = Protocols::QUIT_REQUEST;
	req->length = 12;
	req->status = 0;
	this->emit(req, sizeof(struct message_s));

	struct message_s* res = this->readHeader(Protocols::QUIT_REPLY);
	cout << "Thank you.\n";
	close(sd);
	free(res);
	exit(0);
}
void Client::impl(struct message_s* req, std::vector<string> tokens) {
	if (this->state == Client::INITIAL) {
		//cout << "initial\n";
		if (tokens.size() == 3 && tokens[0] == "open") {
			//open ip port
			OPEN_CONN_REQUEST(req, tokens);
			return;
		}
	} else if (this->state != Client::INITIAL && tokens.size() == 1
			&& tokens[0] == "quit") {
		//quit
		QUIT_REQUEST(req, tokens);
		return;
	} else if (this->state == Client::CONNECTED) {
		if (tokens.size() == 3 && tokens[0] == "auth") {
			//auth user pass
			//Protocols::AUTH_REQUEST
			AUTH_REQUEST(req, tokens);
			return;
		}
	} else if (this->state == Client::AUTHED) {
		if (tokens.size() == 1 && tokens[0] == "ls") {
			//ls
			LIST_REQUEST(req, tokens);
			return;
		} else if (tokens.size() == 2) {
			if (tokens[0] == "get") {
				GET_REQUEST(req, tokens);
				return;
			} else if (tokens[0] == "put") {
				//put file
				PUT_REQUEST(req, tokens);
				return;
			}
		}
	}

	this->log("[ERROR] unexpected command\n");
}
void Client::loop() {
	struct message_s* req = new struct message_s;
	memcpy(req->protocol, this->protocol, 6);
	string text;
	while (1) {
		try {
			//simple cli interface
			cout << "Client> ";
			getline(std::cin, text);
			std::vector<string> tokens = explode(text, ' ');
			//debug start
			/*for (std::vector<int>::size_type i = 0; i != tokens.size(); i++) {
			 cout << tokens[i] << "\n";
			 }
			 cout << this->state << " " << tokens.size() << "\n";
			 */
			//debug end
			this->impl(req, tokens);
		} catch (int e) {
			switch (e) {
			case Exceptions::PROTOCOL_NOT_MATCHED:
				this->log("[ERROR] unknown protocol\n");
				break;
			case Exceptions::UNEXPECTED_TYPE:
				this->log("[ERROR] unexpected type\n");
				break;
			case Exceptions::SOCKET_RECV_ERROR:
			case Exceptions::SOCKET_SEND_ERROR:
			case Exceptions::SOCKET_DISCONNECTED:
				this->log("socket error, disconnected");
				this->state = Client::INITIAL;
				close(this->sd);
				return;
			}
		}

	}
}

void ConnectionHandler::loop() {

}
/*
 void ConnectionHandler::loop() {
 printf("test\n");
 while (1) {
 try {
 printf("BEFORE read header\n");
 struct message_s* msg = this->readHeader();
 char* payload = NULL;
 if (msg->length > 12) {
 payload = this->readPayload(
 msg->length - sizeof(struct message_s));
 this->log("read payload");
 }
 printf("after read header\n");
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
 }
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
 */

int main(int argc, char** argv) {
	Client c;
	c.start();
	/*
	 int sd = socket(AF_INET, SOCK_STREAM, 0);
	 struct sockaddr_in server_addr;
	 memset(&server_addr, 0, sizeof(server_addr));
	 server_addr.sin_family = AF_INET;
	 server_addr.sin_addr.s_addr = inet_addr(argv[1]);
	 server_addr.sin_port = htons(PORT);
	 if (connect(sd, (struct sockaddr *) &server_addr, sizeof(server_addr))
	 < 0) {
	 printf("connection error: %s (Errno:%d)\n", strerror(errno), errno);
	 exit(0);
	 }
	 unsigned char* x = new unsigned char[6] { 0xe3, 'm', 'y', 'f', 't', 'p' };
	 int counter = 0;
	 while (1) {
	 char buff[100];
	 memset(buff, 0, 100);
	 struct message_s msg;
	 scanf("%s", buff);

	 memcpy(&msg.protocol, x, 6);
	 msg.length = 12 + 4;
	 msg.status = counter++;
	 msg.type = 0x00;
	 printf("set buffer\n");
	 memcpy(buff, &msg, sizeof(struct message_s));
	 buff[12] = 'w';
	 buff[13] = 't';
	 buff[14] = 'f';
	 buff[15] = '\0';
	 //scanf("%s", buff);
	 //printf("%s\n",buff);
	 int len;
	 int send_len = msg.length; //strlen(buff)
	 if ((len = send(sd, buff, send_len, 0)) <= 0) {
	 printf("Send Error: %s (Errno:%d)\n", strerror(errno), errno);
	 exit(0);
	 }
	 if (strcmp(buff, "exit") == 0) {
	 close(sd);
	 break;
	 }
	 }
	 */
	return 0;
}
