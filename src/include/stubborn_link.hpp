#pragma once

#include <sys/socket.h>
#include <arpa/inet.h>
#include "message.hpp"

class StubbornLink {
private:
    int _sockfd;
    struct sockaddr_in _addr{};

public:
    StubbornLink(in_addr_t addr, uint16_t port);
    ~StubbornLink();

    int sockfd() const;
    const struct sockaddr_in& addr() const;
    void send(const Message& m, sockaddr_in& q_addr);
    struct sockaddr_in deliver(Message& m);
};

