#include <iostream>
#include <stdio.h>
#include <string.h>
#include <exception>
#include <string>
#include <vector>
#include <sstream>
#include <utility>
#include "protocols.h"
using namespace std;
using namespace ftp;

ConnectionHandler::ConnectionHandler(int sd, struct sockaddr_in address) {
	this->sd = sd;
	this->addr = address;
	this->state = ConnectionHandler::INITIAL;
	this->protocol = new unsigned char[6] { 0xe3, 'm', 'y', 'f', 't', 'p' };
}

void ConnectionHandler::log(const char* str) {
	printf("[%d][%s:%d]%s\n", this->sd, inet_ntoa(this->addr.sin_addr),
			this->addr.sin_port, str);
}

int ConnectionHandler::receive(void* b, size_t len) {
	unsigned int total = 0;
	//assumption buff is large enough to hold size len of data

	while (total < len) {
		unsigned int remain = len - total;
		int received = 0;
		if ((received = recv(this->sd, this->temp,
				remain < sizeof(this->temp) ? remain : sizeof(this->temp), 0))
				<= 0) {
			//may be client shutdown or lost connection
			throw Exceptions::SOCKET_RECV_ERROR;
			printf("receive error: %s (Errno:%d)\n", strerror(errno), errno);
			return -1;
		}

		memcpy(b + total, this->temp, received);
		total += received;
		if (len - total <= 0) {
			return total;
		}
	}
}

int ConnectionHandler::emit(const void* buff, size_t len) {
	int sendLen;
	if ((sendLen = send(sd, buff, len, 0)) <= 0) {
		printf("Send Error: %s (Errno:%d)\n", strerror(errno), errno);
		throw Exceptions::SOCKET_SEND_ERROR;
	}
	return sendLen;
}
message_s* ConnectionHandler::readHeader() {
	this->receive(this->buff, 12);
	message_s* x = new message_s;
	memcpy(x, this->buff, sizeof(struct message_s));
	if (memcmp(x->protocol, this->protocol, 6) != 0) {
		throw Exceptions::PROTOCOL_NOT_MATCHED;
		return x;
	} else if (x->length < 12) {
		throw Exceptions::PROTOCOL_INVALID_SIZE;
	} else {
		return x;
	}
}

char* ConnectionHandler::readPayload(size_t len) {
	char* buff = new char[len];
	int size = this->receive(buff, len);
	return buff;
}

IncomingMessage::IncomingMessage(ConnectionHandler * sock, message_s * header) {
	this->socket = sock;
	this->header = header;
}

//override this and implement the logic
void IncomingMessage::impl() {

}

void IncomingMessage::dispatch(OutgoingMessage* msg) {
	msg->writeHeader(socket);
	msg->writePayload(socket);
}

//helper function
std::vector<std::string> explode(std::string const & s, char delim)
{
    std::vector<std::string> result;
    std::istringstream iss(s);

    for (std::string token; std::getline(iss, token, delim); )
    {
        result.push_back(token);
    }

    return result;
}

