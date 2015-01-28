
#include <iostream>
#include <stdio.h>
#include "protocols.h"
using namespace std;
using namespace ftp;

ConnectionHandler::ConnectionHandler(int sd, struct sockaddr_in address) {
	this->sd = sd;
	this->addr = address;
}



int ConnectionHandler::receive(void* buff, size_t len) {
	return -1;
}
int ConnectionHandler::send(const void* buff, size_t len) {
	return -1;
}
message_s ConnectionHandler::readHeader() {
	this->receive(this->buff, 12);
	message_s x;
	return x;
}


IncomingMessage::IncomingMessage(ConnectionHandler* sock, message_s* header) {
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

