#pragma once

#include <string>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "Define.hpp"
#include "Room.hpp"

using std::string;
using std::vector;

class Client;
class Room;

class Server
{
public:
	Server(const string &port, const string &password);
	virtual ~Server();
	void run();
	void join(C_VECT_STR_R params, Client &client);
	void part(C_VECT_STR_R params, Client &client);
	void privmsg(C_VECT_STR_R params, Client &client);
	void op(C_VECT_STR_R params, Client &client);

	Room &getRoom(const string &name){
		vector<Room>::iterator it = this->channels.begin();
		for (; it != this->channels.end(); ++it)
		{
			if (it->getName() == name)
				return *it;
		}
		return *it;
	}

private:
	Server();
	Server(const Server &other);
	Server &operator=(const Server &other);
	void initSocket();
	void runCommand(const string &command, Client &client);
	void hexChatEntry(VECT_STR &, Client &);

	int port;
	string password;
	int _socket;
	struct sockaddr_in address;
	struct sockaddr_in clientAddress;

	fd_set readfds;
	fd_set writefds;
	/*
		The reason for storing file descriptors in supplementary sets (this->readFdsSup and this->writeFdsSup)
		instead of directly modifying the original sets (this->readFds and this->writeFds) is to avoid race conditions
		and to ensure that the original sets remain unchanged for the next iteration of the loop.


		avoid blocking sockets we need to use select() function
		and copy the original sets to supplementary sets
	*/
	fd_set writeFdsCopy;
	fd_set readFdsCopy;

	vector<Client> clients;
	vector<Room> channels;

	char buffer[1024];
};
