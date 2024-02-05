#include "../include/Client.hpp"
#include "../include/Executor.hpp"
#include "../include/Exception.hpp"
#include "../include/Client.hpp"
#include "../include/Server.hpp"
#include "../include/Room.hpp"
#include "../include/Utils.hpp"

#include <iostream>
#include <string>
#include <sstream>
#include <vector>

using namespace std;

#define JOIN_RESPONSE(nick, ip, channel) ":" + nick + "!" + nick + "@" + ip + " JOIN " + channel + "\r\n"
#define RPL_TOPIC(nick, ip, channel, topic) ":" + nick + "!" + nick + "@" + ip + " TOPIC " + channel + " :" + topic + "\r\n"
#define ERR_CHANNELISFULL(source, channel)			": 471 " + source + " " + channel + " :Cannot join channel (+l)" + "\r\n"           //JOIN
#define ERR_BADCHANNELKEY(source, channel)			": 475 " + source + " " + channel + " :Cannot join channel (+k)" + "\r\n"           //JOIN

void Server::join(C_STR_REF params, Client &client)
{
	stringstream ss(params);
	string roomName, key, message;
	if (!params.empty()){
		ss >> roomName;
		ss >> key;
		if (roomName.empty()){
			FD_SET(client.getFd(), &writefds);
			client.getmesagesFromServer().push_back("JOIN :Not enough parameters\n\r");
			return;
		}
		if (roomName[0] == '#'){
			roomName = roomName.substr(1, roomName.size() - 1);
		}
		if (isRoom(roomName)){
			for (vector<Room>::iterator it = channels.begin(); it != channels.end(); it++){
				if (roomName == (*it).getName()){
					if (!it->isClientInChannel(client.getFd())){
						if ((it->getKeycode() & KEY_CODE) && (it->getKeycode() & LIMIT_CODE))
						{
							if (it->getChanelLimit() <= (int) it->getClients().size()) {
								Utils::instaWrite(client.getFd(), ERR_CHANNELISFULL(client.getNick(), roomName));
							}
							else if (it->getKey() != key) {
								Utils::instaWrite(client.getFd(), ERR_BADCHANNELKEY(client.getNick(), roomName));
							}
							else {
								(*it).getClients().push_back(client);
								Utils::instaWrite(client.getFd(), JOIN_RESPONSE(client.getNick(), client._ip , roomName));
								if (!(*it).getTopic().empty()){
									Utils::instaWrite(client.getFd(), RPL_TOPIC(client.getNick(), client._ip , roomName, (*it).getTopic()));
								}
							}
						}
						else if ((it->getKeycode() & KEY_CODE)) {
							if (it->getKey() != key) {
								Utils::instaWrite(client.getFd(), ERR_BADCHANNELKEY(client.getNick(), roomName));
							}
							else {
								(*it).getClients().push_back(client);
								Utils::instaWrite(client.getFd(), JOIN_RESPONSE(client.getNick(), client._ip , roomName));
								if (!(*it).getTopic().empty()){
									Utils::instaWrite(client.getFd(), RPL_TOPIC(client.getNick(), client._ip , roomName, (*it).getTopic()));
								}
							}
						}
						else if ((it->getKeycode() & LIMIT_CODE)) {
							if (it->getChanelLimit() <= (int) it->getClients().size()) {
								Utils::instaWrite(client.getFd(), ERR_CHANNELISFULL(client.getNick(), roomName));
							}
							else {
								(*it).getClients().push_back(client);
								Utils::instaWrite(client.getFd(), JOIN_RESPONSE(client.getNick(), client._ip , roomName));
								if (!(*it).getTopic().empty()){
									Utils::instaWrite(client.getFd(), RPL_TOPIC(client.getNick(), client._ip , roomName, (*it).getTopic()));
								}
							}
						}
						else {
							(*it).getClients().push_back(client);
							Utils::instaWrite(client.getFd(), JOIN_RESPONSE(client.getNick(), client._ip , roomName));
							if (!(*it).getTopic().empty()){
								Utils::instaWrite(client.getFd(), RPL_TOPIC(client.getNick(), client._ip , roomName, (*it).getTopic()));
							}
						}
					} else {
						FD_SET(client.getFd(), &writefds);
						client.getmesagesFromServer().push_back("You already in the chanel\n\r");
					}
					break;
				}
			}
		} else {
			Room room;
			room.setName(roomName);
			room.setOperator(&client);
			room.addClient(client);
			channels.push_back(room);
			Utils::instaWrite(client.getFd(), JOIN_RESPONSE(client.getNick(), client._ip , roomName));
		}
		responseAllClientResponseToGui(client, getRoom(roomName));
	} else {
		FD_SET(client.getFd(), &writefds);
		client.getmesagesFromServer().push_back("JOIN :Not enough parameters");
	}
	//string message = getMessage(params) , roomName, key;
	//std::cout << "$" << message << "$" << std::endl;
	//std::stringstream ss(message);
	//if (!message.empty()){
	//	ss >> roomName;
	//	if (roomName[0] == '#'){
	//		roomName = roomName.substr(1, roomName.size() - 1);
	//	}
	//	ss >> key;
	//	if (isRoom(roomName)){
	//		std::cout << "hay amq" << std::endl;
	//		for (std::vector<Room>::iterator it = channels.begin(); it != channels.end(); it++){
	//			if (roomName == (*it).getName()){
	//				if (!it->isClientInChannel(client.getFd())){
	//					if ((it->getKeycode() & KEY_CODE) && (it->getKeycode() & LIMIT_CODE))
    //                    {
    //                        if (it->getChanelLimit() <= (int) it->getClients().size()) {
	//							FD_SET(client.getFd(), &writefds);
	//							client.getmesagesFromServer().push_back(ERR_CHANNELISFULL(client.getNick(), roomName));
    //                        }
    //                        else if (it->getKey() != key) {
	//							FD_SET(client.getFd(), &writefds);
	//							client.getmesagesFromServer().push_back(ERR_BADCHANNELKEY(client.getNick(), roomName));
    //                        }
    //                        else {
    //                            (*it).getClients().push_back(client);
	//							FD_SET(client.getFd(), &writefds);
	//							client.getmesagesFromServer().push_back(JOIN_RESPONSE(client.getNick(), client._ip , roomName));
    //                            if (!(*it).getTopic().empty()){
	//								FD_SET(client.getFd(), &writefds);
	//								client.getmesagesFromServer().push_back(RPL_TOPIC(client.getNick(), client._ip , roomName, (*it).getTopic()));
	//							}
	//						}
	//					}
	//					else if ((it->getKeycode() & KEY_CODE)) {
	//						if (it->getKey() != key) {
	//							FD_SET(client.getFd(), &writefds);
	//							client.getmesagesFromServer().push_back(ERR_BADCHANNELKEY(client.getNick(), roomName));
	//						}
	//						else {
	//							(*it).getClients().push_back(client);
	//							FD_SET(client.getFd(), &writefds);
	//							client.getmesagesFromServer().push_back(JOIN_RESPONSE(client.getNick(), client._ip , roomName));
	//							if (!(*it).getTopic().empty()){
	//								FD_SET(client.getFd(), &writefds);
	//								client.getmesagesFromServer().push_back(RPL_TOPIC(client.getNick(), client._ip , roomName, (*it).getTopic()));
	//							}
	//						}
    //                    }
	//					else if ((it->getKeycode() & LIMIT_CODE)) {
	//						if (it->getChanelLimit() <= (int) it->getClients().size()) {
	//							FD_SET(client.getFd(), &writefds);
	//							client.getmesagesFromServer().push_back(ERR_CHANNELISFULL(client.getNick(), roomName));
	//						}
	//						else {
	//							(*it).getClients().push_back(client);
	//							FD_SET(client.getFd(), &writefds);
	//							client.getmesagesFromServer().push_back(JOIN_RESPONSE(client.getNick(), client._ip , roomName));
	//							if (!(*it).getTopic().empty()){
	//								FD_SET(client.getFd(), &writefds);
	//								client.getmesagesFromServer().push_back(RPL_TOPIC(client.getNick(), client._ip , roomName, (*it).getTopic()));
	//							}
	//						}
	//					}
	//					else {
	//						(*it).getClients().push_back(client);
	//						FD_SET(client.getFd(), &writefds);
	//						client.getmesagesFromServer().push_back(JOIN_RESPONSE(client.getNick(), client._ip , roomName));
	//						if (!(*it).getTopic().empty()){
	//							FD_SET(client.getFd(), &writefds);
	//							client.getmesagesFromServer().push_back(RPL_TOPIC(client.getNick(), client._ip , roomName, (*it).getTopic()));
	//						}
	//					}
	//				} else {
	//					FD_SET(client.getFd(), &writefds);
	//					client.getmesagesFromServer().push_back("You already in the chanel\n");
	//				}
	//				break;
	//			}
	//		}
	//	} else {
	//		std::cout << "hay amq2" << std::endl;
	//		Room room;
	//		room.setName(roomName);
	//		room.setOperator(&client);
	//		room.addClient(client);
	//		channels.push_back(room);
	//		Utils::instaWrite(client.getFd(), JOIN_RESPONSE(client.getNick(), client._ip , roomName));
	//	}
	//}
	//else {
	//	std::cout << "hay amq3" << std::endl;
	//	FD_SET(client.getFd(), &writefds);
	//	client.getmesagesFromServer().push_back("JOIN :Not enough parameters");
	//}
	//responseAllClientResponseToGui(client, getRoom(roomName));
}
