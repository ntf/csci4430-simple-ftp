#ifndef FTP_PROTOCOLS
#define FTP_PROTOCOLS
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
struct message_s {
	unsigned char protocol[6]; /* protocol magic number (6 bytes) */
	unsigned char type; /* type (1 byte) */
	unsigned char status; /* status (1 byte) */
	unsigned int length; /* length (header + payload) (4 bytes) */
}__attribute__ ((packed));

std::vector<std::string> explode(std::string const & s, char delim);
namespace ftp {

class Server;
class ConnectionHandler {

protected:
	int sd;
	struct sockaddr_in addr;
	char temp[1024];
	unsigned char state;
	unsigned char* protocol;
	Server* server;
public:
	static const unsigned char INITIAL = 0x01;
	static const unsigned char CONNECTED = 0x02;
	static const unsigned char AUTHED = 0x03;

	char buff[1024];
	ConnectionHandler(int sd, struct sockaddr_in address);
	ConnectionHandler();
	void virtual loop();
	void terminate();
	int emit(const void* buff, size_t len);
	int receive(void* buff, size_t len);
	message_s* readHeader(unsigned char expected);
	void setServer(Server* server);
	char* readPayload(size_t len);
	void log(const char* str);

};

class Exceptions {
public:
	static const int UNEXPECTED_TYPE = 0x10;
	static const int PROTOCOL_NOT_MATCHED = 0x11;
	static const int PROTOCOL_INVALID_SIZE = 0x12;
	static const int ACCESS_DENIED = 0x21;

	static const int SOCKET_DISCONNECTED = 0x31;
	static const int SOCKET_RECV_ERROR = 0x32;
	static const int SOCKET_SEND_ERROR = 0x33;

	static const int INTERNAL_ERROR = 0x40;
	static const int IO_ERROR = 0x41;
};
class Protocols {
public:
	static const unsigned char ANY = 0x00;
	static const unsigned char OPEN_CONN_REQUEST = 0xA1;
	static const unsigned char OPEN_CONN_REPLY = 0xA2;
	static const unsigned char AUTH_REQUEST = 0xA3;
	static const unsigned char AUTH_REPLY = 0xA4;
	static const unsigned char LIST_REQUEST = 0xA5;
	static const unsigned char LIST_REPLY = 0xA6;
	static const unsigned char GET_REQUEST = 0xA7;
	static const unsigned char GET_REPLY = 0xA8;
	static const unsigned char PUT_REQUEST = 0xA9;
	static const unsigned char PUT_REPLY = 0xAA;
	static const unsigned char QUIT_REQUEST = 0xAB;
	static const unsigned char QUIT_REPLY = 0xAC;
	static const unsigned char FILE_DATA = 0xFF;
};
class OutgoingMessage {

public:
	void virtual writeHeader(ConnectionHandler* sock);
	void virtual writePayload(ConnectionHandler* sock);
//	virtual ~OutgoingMessage();
};

class IncomingMessage {
	ConnectionHandler* socket;
	message_s* header;
public:
	IncomingMessage(ConnectionHandler* sock, message_s* header);
	void virtual impl();
	void dispatch(OutgoingMessage* msg);
//	virtual ~IncomingMessage();
};
}
#endif
