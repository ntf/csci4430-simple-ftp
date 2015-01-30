#include "protocols.h"
#include <vector>
#include <map>
#include <string>
//struct message_s;

namespace ftp {

class Client: ConnectionHandler {
	std::string username;
	std::string password;
	void OPEN_CONN_REQUEST(struct message_s* req,
			std::vector<std::string> tokens);
	void QUIT_REQUEST(struct message_s* req, std::vector<std::string> tokens);
	void AUTH_REQUEST(struct message_s* req, std::vector<std::string> tokens);
	void LIST_REQUEST(struct message_s* req, std::vector<std::string> tokens);
	void GET_REQUEST(struct message_s* req, std::vector<std::string> tokens);
	void PUT_REQUEST(struct message_s* req, std::vector<std::string> tokens);
public:
	Client();
	void start();
	void setSD(int sd);
	void setState(int state);
	int getState();
	void virtual loop();
	void virtual impl(struct message_s* req, std::vector<std::string> tokens);
};
}

