#include "Server.hpp"
#include "Exception.hpp"
#include "Utils.hpp"
#include "TextEngine.hpp"
#include "Client.hpp"
#include "Executor.hpp"
#include "Room.hpp"
#include <string>
#include <iostream>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include "Define.hpp"
#include <fcntl.h>

using std::cout;

Server::Server(C_STR_REF port, C_STR_REF password)
	: port(0), password(password)
{
	try
	{
		this->port = Utils::ft_stoi(port);
		if (this->port < 0 || this->port > 65535)
		{
			throw Exception("Invalid port");
		}
		initSocket(); // sunucu soketi açılcak hacım.
	}
	catch (const Exception &e)
	{
		throw e;
	}
}

Server::Server(const Server &other)
	: port(other.port), password(other.password)
{
	*this = other;
}

Server &Server::operator=(const Server &other)
{
	if (this != &other)
	{
		this->port = other.port;
		this->password = other.password;
		this->_socket = other._socket;
		this->clients = other.clients;
		this->channels = other.channels;
		this->clientAddress = other.clientAddress;
		this->address = other.address;
		this->readfds = other.readfds;
		this->writefds = other.writefds;
		this->readFdsCopy = other.readFdsCopy;
		this->writeFdsCopy = other.writeFdsCopy;
	}
	return *this;
}

Server::~Server()
{
	for (VECT_ITER_CLI it = clients.begin(); it != clients.end(); it++)
	{
		close(it->getFd());
	}
	if (this->_socket > 0)
	{
		close(this->_socket);
	}
	this->clients.clear();
	this->channels.clear();
	TextEngine::red("Server closed", TextEngine::printTime(cout)) << std::endl;
}

void Server::run()
{
	socklen_t templen = sizeof(sockaddr_in);
	bool isReadyToSelect = true;
	int bytesRead = 0;

	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	FD_ZERO(&readFdsCopy);
	FD_ZERO(&writeFdsCopy);

	FD_SET(this->_socket, &readfds);  // Add socket to readfds set
	// bu kısımda fd_set yok çünkü fd_set write açık olunca (sunucu asılı kalıyor) ve select fonksiyonu sürekli 0 dönüyor
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
			// select: bir thread içerisinde birden fazla soketin okuma ve yazma işlemlerini takip etmek için kullanılır
			// volatile kullanmak bazen sorunlara sebep oluyor bu sebepten dolayı fdleri kopyalayıp
			// onlar üzerinden işlem yapmak daha mantıklı
			if (select(Utils::getMaxFd(clients, this->_socket) + 1, &readFdsCopy, &writeFdsCopy, NULL, 0) < 0)
			{
				throw Exception("Select failed");
			}
			isReadyToSelect = false; // ne zaman veri okumak veya yazmak için hazır olacağımızı belirler
		}
		//kitlenir
		if (FD_ISSET(this->_socket, &this->readFdsCopy))
		{
			int newSocket = accept(this->_socket, (sockaddr *)&clientAddress, &templen); // Accept new connection (yeni kişinin fdsi)
			if (newSocket < 0)
			{
				throw Exception("Accept failed");
			}
			int port = ntohs(clientAddress.sin_port); // big endian to little endian
			Client newClient(newSocket, port); // Create new client
			inet_ntop(AF_INET, &(clientAddress.sin_addr), newClient._ip, INET_ADDRSTRLEN); // Convert IP to string and save it to newClient
			clients.push_back(newClient); // Add new client to clients vector
			FD_SET(newSocket, &readfds); // kullanıcın okuma ucu açılır (hem okuma hem yaz)
			TextEngine::green("New connection from ", TextEngine::printTime(cout)) << newClient._ip << ":" << newClient.getPort() << std::endl;
			isReadyToSelect = true;
			continue;
		}

		// okuma işlemi
		for (VECT_ITER_CLI a = clients.begin(); a != clients.end() && !isReadyToSelect; a++)
		{
			if (FD_ISSET(a->getFd(), &this->readFdsCopy))
			{
				// Read from socket
				bytesRead = read(a->getFd(), this->buffer, 1024);
				// entera basınca \r \n hexchat bunu gönderiyor
				// nc de ise sadece \n gönderiyor
				if (bytesRead <= 0)
				{
					for (VECT_ITER_CHA it = this->channels.begin(); it != this->channels.end(); it++){
						if (it->isClientInChannel(a->getFd())){
							it->removeClient(a->getFd());
						}
					}
					FD_CLR(a->getFd(), &readfds);
					FD_CLR(a->getFd(), &writefds);
					close(a->getFd());
					TextEngine::blue("Client ", TextEngine::printTime(cout)) << a->_ip << ":" << a->getPort() << " disconnected" << std::endl;
					clients.erase(a);
					isReadyToSelect = true;
				}
				else
				{
					this->buffer[bytesRead] = '\0'; // her zaman sonuna \r \n ekliyor
					string msg = this->buffer;
					if (msg == "\n") // sadece enter basınca gelen
					{
						a->setBuffer(a->getBuffer() + msg);
						isReadyToSelect = true;
					}
					if (msg[msg.length() - 1] != '\n') // control d
					{
						a->setBuffer(a->getBuffer() + msg); // control d
						isReadyToSelect = true;
						break; // enter basmadı kullanıcı demmekki devam edebilir komutu yazmaya
					}
					/*
						komutu ele alacan
					*/
					runCommand(a->getBuffer() + msg, *a);
					a->setBuffer("");
					Utils::clearBuffer(this->buffer, 1024);
				}
				isReadyToSelect = true;
				break;
			}
			// isReadyToSelect = true;
		}
		// yazma işlemi
		for (VECT_ITER_CLI a = clients.begin(); a != clients.end() && !isReadyToSelect; ++a)
		{
			if (FD_ISSET(a->getFd(), &this->writeFdsCopy))
			{
				// bu komutun amacı mesajı gönderdikten sonra mesajı silmek ve diğer mesajı göndermek bunun yerine direkt olarak yollasak nasıl olur?
				// direkt yollasak kapalı fd suspendfdye yazmak ister (ctrl+z) ve seg fault yer
				int bytesWritten = write(a->getFd(), a->getmesagesFromServer()[0].c_str(), a->getmesagesFromServer()[0].length());
				//write(1, "cemal\0", 6);
				a->getmesagesFromServer().erase(a->getmesagesFromServer().begin());
				if (bytesWritten < 0)
				{
					throw Exception("Write failed");
				}
				if (a->getmesagesFromServer().empty()){
					FD_CLR(a->getFd(), &writefds); // askıya alınmış bir soketin yazma ucu kapatılır
				}
				if (bytesWritten == 0)
				{
					for (VECT_ITER_CHA it = this->channels.begin(); it != this->channels.end(); it++){
						if (it->isClientInChannel(a->getFd())){
							it->removeClient(a->getFd()); // kullanıcıyı odadan silmek çünkü ikiside farklı objeler
						}
					}
					FD_CLR(a->getFd(), &writefds); // askıya alınmış bir soketin yazma ucu kapatılır
					FD_CLR(a->getFd(), &readfds); // askıya alınmış bir soketin okuma ucu kapatılır
					close(a->getFd()); // soket kapatılır
					this->clients.erase(a);
					TextEngine::blue("Client ", TextEngine::printTime(cout)) << a->_ip << ":" << a->getPort() << " disconnected" << std::endl;
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
	this->_socket = socket(AF_INET, SOCK_STREAM, 0); // Create socket 0 for macos and IPPROTO_IP for linux
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
		TextEngine::green("Socket created successfully! ", TextEngine::printTime(cout)) << std::endl;
	}
	int dumb = 1;
	// setsockopt: Sets socket options
	// SOL_SOCKET: Socket level : Socket options apply to the socket layer
	// SO_REUSEADDR: Reuse address : Allows other sockets to bind() to this port, unless there is an active listening socket bound to the port already
	// &opt: Option value // NULL
	// sizeof(int): Option length // NULL
	if (setsockopt(this->_socket, SOL_SOCKET, SO_REUSEADDR, &dumb, sizeof(int)) < 0) //hemen portu sal
	{
		throw Exception("Socket option failed");
	}
	else
	{
		TextEngine::green("Socket option set successfully! ", TextEngine::printTime(cout)) << std::endl;
	}
	fcntl(this->_socket, F_SETFL, O_NONBLOCK); // Set socket to non-blocking f bekleme yapmadan devam et
	memset(&address, 0, sizeof(address)); // Zeroing address
	address.sin_family = AF_INET;		  // IPv4
	address.sin_addr.s_addr = INADDR_ANY; // TCP
	address.sin_port = htons(this->port); // ENDIANNESS

	// 10010 -- 2
	// Big Endinian :  1 2 3 4 5
	// Little Endian : 5 4 3 2 1

	// htons: Host to network short
	// bind socket to port
	if (::bind(this->_socket, (struct sockaddr *)&address, sizeof(address)) < 0) // soket port bağlama
	{
		throw Exception("Socket bind failed");
	}
	else
	{
		TextEngine::green("Socket binded successfully! ", TextEngine::printTime(cout)) << std::endl;
	}

	/*
	 * Maximum queue length specifiable by listen.
	*/
	if (listen(this->_socket, SOMAXCONN) < 0) // soket dinlemeye başlar
	{
		throw Exception("Socket listen failed");
	}
	else
	{
		TextEngine::green("Socket listening successfully! ", TextEngine::printTime(cout)) << 	std::endl;
	 }
}

void Server::runCommand(C_STR_REF command, Client &client)
{
	TextEngine::underline("----------------\n", cout);
	TextEngine::cyan("Input is : " + command, cout);

	//cout << trimmed << "#" << std::endl;
	/*
		CAP LS 302
		PASS 123
		NICK asd
		USER asd asd asd :asd
		#
	*/

	VECT_STR softSplit = Utils::ft_split(command, "\n");
	if (softSplit.size() == 0) return;
	// iki splitin amacı \n ile gelen mesajları parçalamak
	for (size_t i = 0; i < softSplit.size(); i++)
	{
		string trimmedLine = Utils::ft_trim(softSplit[i], " \t\r");
		if (trimmedLine.empty()) return;
		VECT_STR splitFirst = Utils::ft_firstWord(trimmedLine); // kelimeyi ayırır komut ve parametreleri
		if (splitFirst.size() <= 1 && !Utils::isEqualNonSensitive(splitFirst[0], "list")) return;
		if (Utils::isEqualNonSensitive(splitFirst[0], "pass"))
		{
			Executor::pass(splitFirst[1], client, this->password); // doğru
		}
		else if (Utils::isEqualNonSensitive(splitFirst[0], "cap"))
		{
			Executor::cap(splitFirst[1], client); // doğru
		}
		else if (Utils::isEqualNonSensitive(splitFirst[0], "quit"))
		{
			this->quit(client);
		}
		else if (client.getIsPassworded() == false){
			Utils::instaWrite(client.getFd(), "First you need to pass the password\n\r");
			//client.getmesagesFromServer().push_back("First you need to pass the password\n\r");
			//FD_SET(client.getFd(), &writefds);
		}
		else if (Utils::isEqualNonSensitive(splitFirst[0], "user"))
		{
			Executor::user(splitFirst[1], client); // doğru
		}
		else if (Utils::isEqualNonSensitive(splitFirst[0], "nick"))
		{
			nick(splitFirst[1], client, writefds); // doğru
		}
		else if (client.getIsRegistered() == false){
			Utils::instaWrite(client.getFd(), "First you need to register\n\r");
		}
		else if (Utils::isEqualNonSensitive(splitFirst[0], "join"))
		{
			this->join(splitFirst[1], client); // doğru
		}
		else if (Utils::isEqualNonSensitive(splitFirst[0], "part"))
		{
			this->part(splitFirst[1], client); // doğru
		}
		else if (Utils::isEqualNonSensitive(splitFirst[0], "op"))
		{
			this->op(splitFirst[1], client);
		}
		else if (Utils::isEqualNonSensitive(splitFirst[0], "mode")){
			this->mode(splitFirst[1], client);
		}
		else if (Utils::isEqualNonSensitive(splitFirst[0], "ping")){
			this->ping(splitFirst[1], client); // doğru
		}
		else if (Utils::isEqualNonSensitive(splitFirst[0], "privmsg"))
		{
			this->privmsg(splitFirst[1], client); // doğru
		}
		else if (Utils::isEqualNonSensitive(splitFirst[0], "who"))
		{
			this->who(splitFirst[1], client); // doğru (iyi test et)
		}
		else if (Utils::isEqualNonSensitive(splitFirst[0], "topic"))
		{
			this->topic(splitFirst[1], client); // doğru
		}
		else if (Utils::isEqualNonSensitive(splitFirst[0], "whois"))
		{
			this->whois(splitFirst[1], client); // doğru
		}
		else if (Utils::isEqualNonSensitive(splitFirst[0], "pong"))
		{
			this->pong(splitFirst[1], client); // doğru
		}
		else if (Utils::isEqualNonSensitive(splitFirst[0], "list"))
		{
			this->list(client); // doğru değil
		}
		else if (Utils::isEqualNonSensitive(splitFirst[0], "names"))
		{
			this->names(client, splitFirst[1]); // doğru
		}
		else if (Utils::isEqualNonSensitive(splitFirst[0], "notice"))
		{
			this->notice(splitFirst[1], client); // doğru
		}
		else if (Utils::isEqualNonSensitive(splitFirst[0], "kick"))
		{
			this->kick(splitFirst[1], client);
		}
		else
		{
			Utils::instaWrite(client.getFd(), "Invalid command\n\r");
			//client.getmesagesFromServer().push_back("Invalid command\n");
			//FD_SET(client.getFd(), &writefds);
		}
	}
	hexChatEntry(softSplit, client);
}


// çoklu komut geldiği için hexchatten giriş olunca buraya düşüyor
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
			TextEngine::red("Client ", TextEngine::printTime(cout)) << client._ip << ":" << client.getPort() << " disconnected" << std::endl;
			//bu kısmı düzgün erase ile yap
			for (VECT_ITER_CLI it = clients.begin(); it != clients.end(); ++it){
				if (it->getFd() == client.getFd()){
					clients.erase(it);
					break;
				}
			}
		}
	}
}

void Server::responseAllClientResponseToGui(Client &client, Room &room)  {
	string message = "";
	for (VECT_ITER_CLI it = room.getClients().begin(); it != room.getClients().end(); it++){
		if (room.isOperator(*it))
			message += "@";
		message += (*it).getNick() + " ";
	}
	Utils::instaWriteAll(room.getClients(), RPL_NAMREPLY(client.getNick(), room.getName(), message));
	Utils::instaWriteAll(room.getClients(), RPL_ENDOFNAMES(client.getNick(), room.getName()));
}

Client &Server::getClientByNick(C_STR_REF nick){
	VECT_ITER_CLI it = this->clients.begin();
	for (; it != this->clients.end(); ++it)
	{
		if (it->getNick() == nick)
			return *it;
	}
	return *it;
}


Room	&Server::getRoom(C_STR_REF roomName){
	VECT_ITER_CHA it = this->channels.begin();
	for (; it != this->channels.end(); ++it)
	{
		if (it->getName() == roomName)
			return *it;
	}
	return *it;
}

vector<Room> &Server::getRooms(){
	return this->channels;
}

bool	Server::isRoom(C_STR_REF roomName){
	VECT_ITER_CHA it = this->channels.begin();
	for (; it != this->channels.end(); ++it)
	{
		if (it->getName() == roomName)
			return true;
	}
	return false;
}

void	Server::addRoom(const Room &room){
	this->channels.push_back(room);
}

void	Server::addClient(const Client &client){
	this->clients.push_back(client);
}

vector<Client> &Server::getClients(){
	return this->clients;
}

bool	Server::isClientInRoom(Room &room, const Client &client){
	VECT_ITER_CLI it = room.getClients().begin();
	for (; it != room.getClients().end(); ++it)
	{
		if (it->getNick() == client.getNick())
			return true;
	}
	return false;
}

bool	Server::isClientInRoom(Client &client, string &room){
	VECT_ITER_CHA it = this->channels.begin();
	for (; it != this->channels.end(); ++it)
	{
		if (it->getName() == room)
		{
			VECT_ITER_CLI cit = it->getClients().begin();
			for (; cit != it->getClients().end(); ++cit)
			{
				if (cit->getNick() == client.getNick())
					return true;
			}
		}
	}
	return false;
}

void	Server::removeClient(int fd){
	VECT_ITER_CLI it = this->clients.begin();
	for (; it != this->clients.end(); ++it)
	{
		if (it->getFd() == fd)
		{
			this->clients.erase(it);
			return;
		}
	}
}

