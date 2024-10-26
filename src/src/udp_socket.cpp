#include <string>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "udp_socket.hpp"

UDPSocket::UDPSocket(in_addr_t addr, uint16_t port) {
  // Create a non-blocking UDP socket.
  _sockfd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
  if (_sockfd < 0) {
    perror("socket creation failed");
    exit(EXIT_FAILURE);
  }

  std::memset(&_addr, 0, sizeof(_addr));
  _addr.sin_family = AF_INET;
  // Already converted to network byte order.
  _addr.sin_port = port;
  _addr.sin_addr.s_addr = addr;

//  std::cerr << "[DEBUG] Binding to " << inet_ntoa(_addr.sin_addr) << ":" << ntohs(_addr.sin_port) << std::endl;

  // Allow reusing the same address and port for multiple sockets.
  int optval = 1;
  if (setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) < 0) {
    perror("setsockopt(SO_REUSEADDR) failed");
    exit(EXIT_FAILURE);
  }
  if (setsockopt(_sockfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(int)) < 0) {
    perror("setsockopt(SO_REUSEPORT) failed");
    exit(EXIT_FAILURE);
  }

  if (bind(_sockfd, reinterpret_cast<struct sockaddr*>(&_addr), sizeof(_addr)) < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }
}

UDPSocket::~UDPSocket() {
  close(_sockfd);
}

int UDPSocket::sockfd() const {
  return _sockfd;
}

const struct sockaddr_in& UDPSocket::addr() const {
  return _addr;
}

void UDPSocket::conn(const struct sockaddr_in& addr) {
//  std::cerr << "[DEBUG] Connecting to " << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port) << std::endl;
  if (connect(_sockfd, reinterpret_cast<const struct sockaddr*>(&addr), sizeof(addr)) < 0) {
    std::cerr << "[DEBUG] Connection failed." << std::endl;
    perror("connect failed");
    exit(EXIT_FAILURE);
  }
}

ssize_t UDPSocket::send1(const std::vector<uint8_t>& buffer) const {
  return send(_sockfd, buffer.data(), buffer.size(), 0);
}

ssize_t UDPSocket::receive(std::vector<uint8_t>& buffer) const {
  return recv(_sockfd, buffer.data(), buffer.size(), 0);
}