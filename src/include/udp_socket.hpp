#pragma once

#include <vector>
#include <netinet/in.h>
#include <sys/socket.h>

constexpr static int RECV_BUF_SIZE = 65536;

class UDPSocket {
private:
    int _sockfd;
    struct sockaddr_in _addr;
public:
    UDPSocket(in_addr_t addr, uint16_t port);
    ~UDPSocket();

    int sockfd() const;
    const struct sockaddr_in& addr() const;
    void conn(const struct sockaddr_in& addr);
    ssize_t send1(const std::vector<uint8_t>& buffer) const;
    ssize_t receive(std::vector<uint8_t>& buffer) const;
};
