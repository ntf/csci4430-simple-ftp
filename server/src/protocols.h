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

namespace ftp {

class ConnectionHandler {
	int sd;
	struct sockaddr_in addr;
	char buff[1024];
	int state = 0;
public:
	ConnectionHandler(int sd, struct sockaddr_in address);
	void virtual loop();
	int send(const void* buff, size_t len);
	int receive(void* buff, size_t len);
	message_s readHeader();
};

class Protocols {
public:
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
