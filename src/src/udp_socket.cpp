#include <string>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "udp_socket.hpp"

UDPSocket::UDPSocket(in_addr_t addr, uint16_t port) {
  // Create a non-blocking UDP socket.
  _outfd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
  if (_outfd < 0) {
    perror("socket creation failed");
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in sock_addr{};
  std::memset(&sock_addr, 0, sizeof(sock_addr));
  sock_addr.sin_family = AF_INET;
  // Already converted to network byte order.
  sock_addr.sin_port = port;
  sock_addr.sin_addr.s_addr = addr;

  // Allow reusing the same address and port for multiple sockets.
  int optval = 1;
  if (setsockopt(_outfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) < 0) {
    perror("setsockopt(SO_REUSEADDR) failed");
    exit(EXIT_FAILURE);
  }
  if (setsockopt(_outfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(int)) < 0) {
    perror("setsockopt(SO_REUSEPORT) failed");
    exit(EXIT_FAILURE);
  }

  if (bind(_outfd, reinterpret_cast<struct sockaddr*>(&sock_addr), sizeof(sock_addr)) < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  _infd = dup(_outfd);
}

UDPSocket::~UDPSocket() {
  close(_outfd);
  close(_infd);
}

void UDPSocket::set_blocking_socket(bool blocking, int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    perror("fcntl F_GETFL failed");
    exit(EXIT_FAILURE);
  }

  if (blocking) {
    flags &= ~O_NONBLOCK;
  } else {
    flags |= O_NONBLOCK;
  }

  if (fcntl(fd, F_SETFL, flags) == -1) {
    perror("fcntl F_SETFL failed");
    exit(EXIT_FAILURE);
  }
}

void UDPSocket::set_blocking_input(bool blocking) const {
  set_blocking_socket(blocking, _infd);
}

void UDPSocket::set_blocking_output(bool blocking) const {
  set_blocking_socket(blocking, _outfd);
}

int UDPSocket::infd() const {
  return _infd;
}

int UDPSocket::outfd() const {
  return _outfd;
}

void UDPSocket::conn(const struct sockaddr_in& addr) {
  if (connect(_outfd, reinterpret_cast<const struct sockaddr*>(&addr), sizeof(addr)) < 0) {
    perror("connect failed");
    exit(EXIT_FAILURE);
  }
}

ssize_t UDPSocket::send_buf(const std::vector<uint8_t>& buffer) const {
  return send(_outfd, buffer.data(), buffer.size(), 0);
}

ssize_t UDPSocket::recv_buf(std::vector<uint8_t>& buffer) const {
  return recv(_infd, buffer.data(), buffer.size(), 0);
}