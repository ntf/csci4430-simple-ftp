#include "protocols.h"
#include <vector>
#include <map>
#include <string>
#include <pthread.h>
//struct message_s;

namespace ftp {

//class ConnectionHandler;
struct ConnectionThread {
	pthread_t thread;
	ConnectionHandler* socket;
};
class Server {
	int sd;
	int port;
	int connQueue;
	struct sockaddr_in serverAddress;
	std::vector<ConnectionThread*> sockets;

public:
	std::map<std::string, std::string> credentials;
	unsigned char* protocol;
	Server(int port);
	void start();
	void loop();
	void log(const char* str);
};
}
