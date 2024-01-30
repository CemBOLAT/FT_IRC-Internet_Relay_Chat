#pragma once

#include <string>
#include <vector>

class Client;
using namespace std;

class Room{
	public:
		Room(Client *_c, string name);
		~Room() {}

		void				addClient(Client *client) { clients.push_back(client); }
		void				removeClient(Client *client);
		void				setAdmin(Client *client) { admin = client; }
		void				setName(const string &name) { this->name = name; }
		void				setIsPrivate(bool isPrivate) { this->isPrivate = isPrivate; }
		void				setClientCount(int clientCount) { this->clientCount = clientCount; }
		void				setMaxClientCount(int maxClientCount) { this->maxClientCount = maxClientCount; }

		Client				*getAdmin() const { return admin; }
		string				getName() const { return name; }
		bool				getIsPrivate() const { return isPrivate; }
		int					getClientCount() const { return clientCount; }
		int					getMaxClientCount() const { return maxClientCount; }
		vector<Client *>	getClients() const { return clients; }

	private:
		int					clientCount;
		int					maxClientCount;
		vector<Client *>	clients;
		Client				*admin;
		string				name;
		bool				isPrivate;

		Room();
		Room(const Room &room);
		Room &operator=(const Room &room);
};
