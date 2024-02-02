#pragma once

#include <string>
#include <netinet/in.h>

using std::string;

// #include in.h

class Client
{
public:
    Client(int fd, int port) : _fd(fd), _port(port), isRegistered(false), isPassworded(false) {}
    Client() : _fd(-1), _port(-1), isRegistered(false), isPassworded(false) {}
    Client(const Client &other) : _fd(other._fd), _port(other._port) {}
    Client &operator=(const Client &other)
    {
        if (this != &other)
        {
            _fd = other._fd;
            _port = other._port;
        }
        return *this;
    }

    int &getFd() { return _fd; }
    int &getPort() { return _port; }
    string &getBuffer() { return buffer; }
    bool &getIsRegistered() { return isRegistered; }
    bool &getIsPassworded() { return isPassworded; }
    int &getType() { return _type; }
    vector<string> &mesagesFromServer() { return _messagesFromServer; }

    void setRegistered(bool val) { isRegistered = val; }
    void setPassworded(bool val) { isPassworded = val; }
    void setBuffer(const string &str) { buffer = str; }
    void setType(int type) { _type = type; }
    virtual ~Client() {}
    char _ip[INET_ADDRSTRLEN]; // 123.123.123.123 + \0
private:
    int _type; // 1:hex 2:nc 3:bot
    int _fd;
    int _port;
    string buffer;
    vector<string> _messagesFromServer;
    bool isRegistered;
    bool isPassworded;
};