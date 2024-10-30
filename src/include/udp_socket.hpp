#pragma once

#include <vector>
#include <netinet/in.h>
#include <sys/socket.h>

constexpr static int RECV_BUF_SIZE = 65536;

class UDPSocket {
private:
    int _infd;
    int _outfd;
public:
    UDPSocket(in_addr_t addr, uint16_t port);
    ~UDPSocket();

    int infd() const;
    int outfd() const;
    void conn(const struct sockaddr_in& addr);
    ssize_t send_buf(const std::vector<uint8_t>& buffer) const;
    ssize_t recv_buf(std::vector<uint8_t>& buffer) const;
};
