all: 	myftpclient myftpserver
myftpclient: myftpclient.cpp myftpclient.h protocols.cpp protocols.h
	g++ -Wall myftpclient.cpp protocols.cpp -o myftpclient	
myftpserver: myftpserver.cpp myftpserver.h protocols.cpp protocols.h
	g++ -Wall myftpserver.cpp protocols.cpp -o myftpserver -lpthread


