#include "../include/Server.hpp"
#include "../include/Exception.hpp"
#include "../include/Utils.hpp"
#include "../include/TextEngine.hpp"
#include "../include/Client.hpp"
#include "../include/Executor.hpp"
#include "../include/Room.hpp"
#include <string>
#include <iostream>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include "../include/Define.hpp"

using std::cout;

Server::Server(const std::string &port, const std::string &password)
	: port(0), password(password)
{
	try
	{
		this->port = Utils::ft_stoi(port);
		if (this->port < 0 || this->port > 65535)
		{
			throw Exception("Invalid port");
		}
		initSocket();
	}
	catch (const Exception &e)
	{
		throw e;
	}
}

Server::~Server()
{
	if (this->_socket > 0)
	{
		close(this->_socket);
	}
}

void Server::run()
{
	sockaddr_in clientAddress;
	socklen_t templen = sizeof(sockaddr_in);
	bool isReadyToSelect = true;
	int bytesRead = 0;

	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	FD_ZERO(&readFdsCopy);
	FD_ZERO(&writeFdsCopy);

	FD_SET(this->_socket, &readfds);  // Add socket to readfds set
	//FD_SET(this->_socket, &writefds); // Add socket to writefds set
	while (true)
	{
		while (isReadyToSelect)
		{
			readFdsCopy = readfds;
			writeFdsCopy = writefds;
			/*
				0 - 1 - 2
				3 server socket
				4 is for select function
			*/
			// nullptr is a C++11 feature
			if (select(clients.size() + 4, &readFdsCopy, &writeFdsCopy, NULL, 0) < 0)
			{
				throw Exception("Select failed");
			}
			isReadyToSelect = false;
		}
		if (FD_ISSET(this->_socket, &this->readFdsCopy))
		{
			int newSocket = accept(this->_socket, (sockaddr *)&clientAddress, &templen);
			if (newSocket < 0)
			{
				throw Exception("Accept failed");
			}
			int port = ntohs(clientAddress.sin_port);
			Client newClient(newSocket, port);
			inet_ntop(AF_INET, &(clientAddress.sin_addr), newClient._ip, INET_ADDRSTRLEN); // Convert IP to string and save it to newClient
			clients.push_back(newClient);
			FD_SET(newSocket, &readfds);

			TextEngine::green("New connection from ", cout) << newClient._ip << ":" << newClient.getPort() << std::endl;
			isReadyToSelect = true;
			continue;
		}
		for (VECT_ITER a = clients.begin(); a != clients.end() && !isReadyToSelect; a++)
		{
			if (FD_ISSET(a->getFd(), &this->readFdsCopy))
			{
				// Read from socket
				bytesRead = read(a->getFd(), this->buffer, 1024);
				if (bytesRead <= 0)
				{
					FD_CLR(a->getFd(), &readfds);
					FD_CLR(a->getFd(), &writefds);
					close(a->getFd());
					TextEngine::blue("Client ", cout) << a->_ip << ":" << a->getPort() << " disconnected" << std::endl;
					clients.erase(a);
					isReadyToSelect = true;
				}
				else
				{
					this->buffer[bytesRead] = '\0';
					string msg = this->buffer;
					if (msg == "\n")
					{
						isReadyToSelect = true;
						break; // Continue to next client if message is empty
					}
					if (msg[msg.length() - 1] != '\n')
					{
						a->setBuffer(a->getBuffer() + msg);
						isReadyToSelect = true;
						break;
					}
					/*
						komutu ele alacan
					*/
					runCommand(msg, *a);
					// isReadyToSelect = true;
				}
				isReadyToSelect = true;
				break;
			}
			// isReadyToSelect = true;
		}
		for (VECT_ITER a = clients.begin(); a != clients.end() && !isReadyToSelect; ++a)
		{
			if (FD_ISSET(a->getFd(), &this->writeFdsCopy))
			{
				int bytesWritten = write(a->getFd(), a->getmesagesFromServer()[0].c_str(), a->getmesagesFromServer()[0].length());
				a->getmesagesFromServer().erase(a->getmesagesFromServer().begin());
				if (bytesWritten < 0)
				{
					throw Exception("Write failed");
				}
				if (a->getmesagesFromServer().empty()){
					FD_CLR(a->getFd(), &writefds);
				}
				if (bytesWritten == 0)
				{
					FD_CLR(a->getFd(), &writefds);
					FD_CLR(a->getFd(), &readfds);
					close(a->getFd());
					this->clients.erase(a);
					TextEngine::blue("Client ", cout) << a->_ip << ":" << a->getPort() << " disconnected" << std::endl;
				}
				Utils::clearBuffer(this->buffer, 1024);
				isReadyToSelect = true;
				break;
			}
		}
	}
}

void Server::initSocket()
{
	this->_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP); // Create socket 0 for macos
	// AF_INE IPV4
	// SOCK_STREAM TCP //SOCK_DGRAM UDP
	// TCP: Transmission Control Protocol : Veri paketlerinin karşı tarafa ulaşmasını garanti eder. Yavaştır.
	// UDP: User Datagram Protocol : Veri paketlerinin karşı tarafa ulaşmasını garanti etmez. Hızlıdır.
	if (_socket < 0)
	{
		throw Exception("Socket creation failed");
	}
	else
	{
		TextEngine::green("Socket created successfully", cout) << std::endl;
	}
	int opt = 1;
	// setsockopt: Sets socket options
	// SOL_SOCKET: Socket level : Socket options apply to the socket layer
	// SO_REUSEADDR: Reuse address : Allows other sockets to bind() to this port, unless there is an active listening socket bound to the port already
	// &opt: Option value // NULL
	// sizeof(int): Option length // NULL
	if (setsockopt(this->_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)) < 0)
	{
		throw Exception("Socket option failed");
	}
	else
	{
		TextEngine::green("Socket option set successfully", cout) << std::endl;
	}

	memset(&address, 0, sizeof(address)); // Zeroing address
	address.sin_family = AF_INET;		  // IPv4
	address.sin_addr.s_addr = INADDR_ANY; // TCP
	address.sin_port = htons(this->port); // ENDIANNESS

	// Big Endinian : 1 2 3 4 5
	// Little Endian : 5 4 3 2 1

	// htons: Host to network short
	// bind socket to port
	if (bind(this->_socket, (struct sockaddr *)&address, sizeof(address)) < 0)
	{
		throw Exception("Socket bind failed");
	}
	else
	{
		TextEngine::green("Socket binded successfully", cout) << std::endl;
	}

	/*
	 * Maximum queue length specifiable by listen.
	 */
	if (listen(this->_socket, SOMAXCONN) < 0)
	{
		throw Exception("Socket listen failed");
	}
	else
	{
		TextEngine::green("Socket listening successfully", cout) << std::endl;
	}
}

void Server::runCommand(const std::string &command, Client &client)
{
	string trimmed = Utils::ft_trim(command, " \r");
	cout << trimmed << "#" << std::endl;

	VECT_STR softSplit = Utils::ft_split(trimmed, "\n");
	// iki splitin amacı \n ile gelen mesajları parçalamak
	for (size_t i = 0; i < softSplit.size(); i++)
	{
		VECT_STR params = Utils::ft_split(softSplit[i], " \t\r");
		if (params.size() == 0)
		{
			return;
		}
		if (Utils::isEqualNonSensitive(params[0], "pass"))
		{
			Executor::pass(params, client, this->password, writefds);
		}
		else if (Utils::isEqualNonSensitive(params[0], "cap"))
		{
			Executor::cap(params, client);
		}
		else if (client.getIsPassworded() == false){
			FD_SET(client.getFd(), &writefds);
			client.getmesagesFromServer().push_back("First you need to pass the password\n");
		}
		else if (Utils::isEqualNonSensitive(params[0], "nick"))
		{
			nick(params, client, writefds);
		}
		else if (Utils::isEqualNonSensitive(params[0], "user"))
		{
			Executor::user(params, client, writefds);
		}
		else if (client.getIsRegistered() == false){
			FD_SET(client.getFd(), &writefds);
			client.getmesagesFromServer().push_back("First you need to register\n");
		}
		else if (Utils::isEqualNonSensitive(params[0], "join"))
		{
			this->join(params, client);
		}
		else if (Utils::isEqualNonSensitive(params[0], "part"))
		{
			this->part(params, client);
		}
		else if (Utils::isEqualNonSensitive(params[0], "privmsg"))
		{
			this->privmsg(params, client);
		}
		else if (Utils::isEqualNonSensitive(params[0], "op"))
		{
			this->op(params, client);
		} else if (Utils::isEqualNonSensitive(params[0], "mode")){
			this->mode(params, client);
		} else if (Utils::isEqualNonSensitive(params[0], "ping")){
			this->ping(params, client);
		}
		else
		{
			FD_SET(client.getFd(), &writefds);
			client.getmesagesFromServer().push_back("Invalid command\n");
		}
	}
	hexChatEntry(softSplit, client);
}

void Server::hexChatEntry(VECT_STR &params, Client &client)
{
	if (params[0] != "CAP" && params.size() != 1)
	{
		if (client.getIsPassworded() == false)
		{
			FD_CLR(client.getFd(), &readfds);
			FD_CLR(client.getFd(), &writefds);
			std::cout << "Invalid password" << std::endl;
			close(client.getFd());
			std::cout << "Client " << client._ip << ":" << client.getPort() << " disconnected" << std::endl;
			this->clients.erase(this->clients.begin() + client.getFd() - 4); // maybe wrong
		}
	}
}

#define RPL_NAMREPLY(nick, channel, users)			": 353 " + nick + " = " + channel + " :" + users + "\r\n"
#define RPL_ENDOFNAMES(nick, channel)               ": 366 " + nick + " " + channel + " :End of /NAMES list\r\n"

void Server::responseAllClientResponseToGui(Client &client, Room &room)  {
	string message;
	Room tmp = room;
	if (tmp.getName().empty())
		return;

	std::cout << "********" << std::endl;
	std::cout << room.getName() << std::endl;
	std::cout << room.getClients().size()	<< std::endl;
	std::cout << "********" << std::endl;
	for (std::vector<Client>::iterator it = tmp.getClients().begin(); it != tmp.getClients().end(); it++){
		if (it->getFd() == tmp.getOperator()->getFd())
			message += "@";
		message += (*it).getNick() + " ";
	}
	Utils::instaWriteAll(tmp.getClients(), RPL_NAMREPLY(client.getNick(), room.getName(), message));
	Utils::instaWriteAll(tmp.getClients(), RPL_ENDOFNAMES(client.getNick(), room.getName()));
}


/*

void Server::showRightGui(Client &cli, Chanel &cha) {
    std::string msg;
    Chanel tmp = getChanel(cha.name);
    if (tmp.name.empty())
        return;
    for(std::vector<Client>::iterator it = tmp.clients.begin() ; it != tmp.clients.end(); ++it) {
        if (it->cliFd == tmp.op->cliFd)
            msg += "@";
        msg += (*it).nick + " ";
    }
    Utilities::writeAllRpl(tmp.getFds(), RPL_NAMREPLY(cli.nick, cha.name, msg));
    Utilities::writeAllRpl(tmp.getFds(), RPL_ENDOFNAMES(cli.nick, cha.name));
}

*/
