#include "Server.hpp"
#include "Exception.hpp"
#include "Utils.hpp"
#include "TextEngine.hpp"
#include "Client.hpp"
#include "Executor.hpp"
#include "Room.hpp"
#include "Define.hpp"
#include "Bot.hpp"
#include <string>
#include <iostream>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include <fcntl.h>

#define RPL_ENTRY(password, nick, user) "CAP BOT\nPASS " + password + "\nNICK " + nick + "\nUSER " + user + " 0 * :turco\n"
#define BOT_WELCOME(nick, msg) "PRIVMSG " + nick + " :" + msg + "\r\n"

Bot::Bot(C_STR_REF port, C_STR_REF password) : _port(0), _password(password)
{
	this->_name = "turco";
	try
	{
		this->_port = Utils::ft_stoi(port);
		if (this->_port < 0 || this->_port > 65535)
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

void Bot::initSocket()
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
		TextEngine::green("Socket created successfully", cout) << std::endl;
	}

	memset(&_bot_addr, 0, sizeof(_bot_addr));	   // Zeroing address
	_bot_addr.sin_family = AF_INET;			   // IPv4
	_bot_addr.sin_addr.s_addr = INADDR_ANY;	   // TCP
	_bot_addr.sin_port = htons(this->_port);	   // ENDIANNESS

	// Big Endinian : 1 2 3 4 5
	// Little Endian : 5 4 3 2 1

	// htons: Host to network short
	// bind socket to port
	if (connect(this->_socket, (struct sockaddr *)&_bot_addr, sizeof(_bot_addr)) < 0) //sockete baglanma
	{
		throw Exception("Connection failed");
	}
	else
	{
		TextEngine::green("Connection established", cout) << std::endl;
	}
}



namespace {
    VECT_STR getAnthem() {
        VECT_STR anthem;
        anthem.push_back("Korkma! Sönmez bu şafaklarda yüzen al sancak;");
        anthem.push_back("Sönmeden yurdumun üstünde tüten en son ocak.");
        anthem.push_back("O benim milletimin yıldızıdır, parlayacak;");
        anthem.push_back("O benimdir, o benim milletimindir ancak!");
		
        anthem.push_back("Çatma, kurban olayım çehreni ey nazlı hilâl;");
        anthem.push_back("Kahraman ırkıma bir gül... ne bu şiddet bu celâl?...");

        return anthem;
    }
}

void	Bot::run(){
	VECT_STR	turkishAnthem = getAnthem();
	Utils::instaWrite(this->_socket, RPL_ENTRY(this->_password, this->_name, "user"));
	while (true){
		fd_set	readfds;
		FD_ZERO(&readfds);
		FD_SET(this->_socket, &readfds);
		struct timeval timeout; // Set timeout because select is blocking and we want to check if the socket is ready to read
		timeout.tv_sec = 0;
		timeout.tv_usec = 100000;

		int ret = select(this->_socket + 1, &readfds, NULL, NULL, &timeout);

		if (ret < 0){
			throw Exception("Select failed");
		}
		else if (ret == 0){
			continue;
		}
		else {
			int bytesRead = read(this->_socket, this->_buffer, 1024);
			if (bytesRead < 0){
				throw Exception("Read failed");
			}
			else if (bytesRead == 0){
				continue;
			}
			else {
				_buffer[bytesRead] = '\0';
				VECT_STR messages = Utils::ft_split(_buffer, " ");
				TextEngine::blue(_buffer, TextEngine::printTime(cout)) << std::endl;
				// size control et
				if (messages[1] == "PRIVMSG" || messages[1] == "PING"){
					string	nick = Utils::ft_trim(Utils::ft_getNick(messages[0]), "\r\n");
					if (messages[1] == "PING"){
						Utils::instaWrite(this->_socket, "PONG " + messages[2] + "\r\n");
					}
					else {
						for (size_t i = 0; i < turkishAnthem.size(); i++)
							Utils::instaWrite(this->_socket, BOT_WELCOME(nick, turkishAnthem[i]));
					}
				}
			}
		}
	}
	close(this->_socket);
}
