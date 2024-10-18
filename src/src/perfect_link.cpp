#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include "perfect_link.hpp"

PerfectLink::PerfectLink(const char *ip, uint16_t port) {
    // Create a UDP socket.
    _sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (_sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    std::memset(&_addr, 0, sizeof(_addr));
    _addr.sin_family = AF_INET;
    _addr.sin_port = htons(port);
    _addr.sin_addr.s_addr = inet_addr(ip);

    if (connect(_sockfd, reinterpret_cast<struct sockaddr*>(&_addr), sizeof(_addr)) < 0) {
        perror("connection failed");
        exit(EXIT_FAILURE);
    }
}

PerfectLink::~PerfectLink() {
    close(_sockfd);
}

void PerfectLink::send(const Message& m, const Process& q) {
    sendto(_sockfd, m.data(), m.size(), 0,
           reinterpret_cast<struct sockaddr*>(&q.addr), sizeof(q.addr));
}

void PerfectLink::receive(const Message& m, const Process& p) {
    if (_delivered.find(m) != _delivered.end()) {
        return;
    }
    recvfrom(_sockfd, m.data(), m.size(), 0,
             reinterpret_cast<struct sockaddr*>(&p.addr), sizeof(p.addr));
}