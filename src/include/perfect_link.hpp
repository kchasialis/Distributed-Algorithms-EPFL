#pragma once

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unordered_set>
#include "message.hpp"
#include "process.hpp"

class PerfectLink {
private:
    int _sockfd;
    struct sockaddr_in _addr;
    std::unordered_set<Message, MessageHash, MessageEqual> _sent;
    std::unordered_set<Message, MessageHash, MessageEqual> _delivered;

public:
    PerfectLink(const char *ip, uint16_t port);
    ~PerfectLink();

    void send(const Message& m, const Process& q);
    void receive(const Message& m, const Process& p);
};

