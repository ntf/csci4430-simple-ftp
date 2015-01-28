#include "protocols.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <vector>
#include <map>
#include <cstring>
#include <iostream>


using namespace std;
using namespace ftp;

message_s ConnectionHandler::readHeader() {
	this->receive(this->buff,12);
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

