#include "protocols.h"
//struct message_s;

namespace ftp {

//class ConnectionHandler;

class Server {
	int sd;
	int port;
	int connQueue;
	struct sockaddr_in serverAddress;
	std::vector<ConnectionHandler*> sockets;

public:
	unsigned char* protocol;
	Server(int port);
	void start();
	void loop();

};
}
