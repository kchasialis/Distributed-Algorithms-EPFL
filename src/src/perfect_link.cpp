#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include "perfect_link.hpp"
#include "process.hpp"

PerfectLink::PerfectLink(in_addr_t addr, uint16_t port, bool sender) {
  // Create a UDP socket.
  _sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (_sockfd < 0) {
      perror("socket creation failed");
      exit(EXIT_FAILURE);
  }

  std::memset(&_addr, 0, sizeof(_addr));
  _addr.sin_family = AF_INET;
  // Already converted to network byte order.
  _addr.sin_port = port;
  _addr.sin_addr.s_addr = addr;

  std::cerr << "PerfectLink created with address: " << inet_ntoa(_addr.sin_addr) << " and port: " << ntohs(_addr.sin_port) << std::endl;

  if (!sender) {
      if (bind(_sockfd, reinterpret_cast<struct sockaddr*>(&_addr), sizeof(_addr)) < 0) {
          perror("bind failed");
          exit(EXIT_FAILURE);
      }
  }
}

PerfectLink::~PerfectLink() {
  close(_sockfd);
}

int PerfectLink::sockfd() const {
  return _sockfd;
}

const struct sockaddr_in& PerfectLink::addr() const {
  return _addr;
}

const std::unordered_set<Message, MessageHash, MessageEqual>& PerfectLink::sent() const {
  return _sent;
}

const std::unordered_set<Message, MessageHash, MessageEqual>& PerfectLink::delivered() const {
  return _delivered;
}

void PerfectLink::send(const Message& m, sockaddr_in& q_addr) {
  std::cout << "Sending message with seq_id: " << m.seq_id() << std::endl;

  ssize_t bytes_sent = sendto(_sockfd, m.data(), m.size(), 0,
                              reinterpret_cast<struct sockaddr*>(&q_addr),
                              sizeof(q_addr));
  if (bytes_sent < 0) {
      std::string err_msg = "sendto() failed. Error message: ";
      err_msg += strerror(errno);
      perror(err_msg.c_str());
      exit(EXIT_FAILURE);
  }
  _sent.insert(m);
}

void PerfectLink::deliver(Message& m) {
  if (_delivered.find(m) != _delivered.end()) {
      return;
  }

  struct sockaddr_in sender_addr;
  socklen_t addr_len = sizeof(sender_addr);
  ssize_t bytes_recv = recvfrom(_sockfd, m.data(), m.size(), 0,
                                reinterpret_cast<struct sockaddr*>(&sender_addr),
                                &addr_len);
  if (bytes_recv < 0) {
      std::string err_msg = "recvfrom() failed. Error message: ";
      err_msg += strerror(errno);
      perror(err_msg.c_str());
      exit(EXIT_FAILURE);
  }
  _delivered.insert(m);

  std::cout << "Received message with seq_id: " << m.seq_id() << std::endl;
}