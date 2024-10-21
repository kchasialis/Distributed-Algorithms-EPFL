#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include "stubborn_link.hpp"

StubbornLink::StubbornLink(in_addr_t addr, uint16_t port) {
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

  std::cerr << "[DEBUG-SL] StubbornLink created with address: " << inet_ntoa(_addr.sin_addr) << " and port: " << ntohs(_addr.sin_port) << std::endl;
  std::cerr << "[DEBUG-SL] StubbornLink port: " << port << std::endl;

  if (bind(_sockfd, reinterpret_cast<struct sockaddr*>(&_addr), sizeof(_addr)) < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }
}

StubbornLink::~StubbornLink() {
  close(_sockfd);
}

int StubbornLink::sockfd() const {
  return _sockfd;
}

const struct sockaddr_in& StubbornLink::addr() const {
  return _addr;
}

void StubbornLink::send(const Message& m, sockaddr_in& q_addr) {
  // Set timeout to 3 seconds.
  struct timeval timeout{};
  timeout.tv_sec = 3;
  timeout.tv_usec = 0;

  if (setsockopt(_sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
    perror("setsockopt failed");
    exit(EXIT_FAILURE);
  }

  // Keep sending the message with a timeout until an ACK is received.
  while (true) {
    std::cerr << "[DEBUG:Send] Sending message with id: " << m.seq_id()  << std::endl;

    ssize_t bytes_sent = sendto(_sockfd, m.data(), m.size(), 0,
                                reinterpret_cast<struct sockaddr*>(&q_addr),
                                sizeof(q_addr));
    if (bytes_sent < 0) {
      std::string err_msg = "sendto() failed. Error message: ";
      err_msg += strerror(errno);
      perror(err_msg.c_str());
      exit(EXIT_FAILURE);
    }

    std::cerr << "[DEBUG:Send] Waiting for ACK..." << std::endl;

    // Wait for ACK.
    Message ack{};
    ssize_t bytes_recv = recvfrom(_sockfd, ack.data(), ack.size(), 0, nullptr, nullptr);
    if (bytes_recv < 0) {
      if (errno == EWOULDBLOCK) {
        std::cerr << "[DEBUG:Send] No ACK received, retrying...\n";
        continue;
      } else {
        std::string err_msg = "recvfrom() failed. Error message: ";
        err_msg += strerror(errno);
        perror(err_msg.c_str());
        exit(EXIT_FAILURE);
      }
    } else {
      std::cout << "[DEBUG:Send] Received ACK for seq_id: " << ack.seq_id() << std::endl;
      break;
    }
  }

  // Reset timeout to blocking.
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;
  if (setsockopt(_sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
    perror("setsockopt failed");
    exit(EXIT_FAILURE);
  }
}

void StubbornLink::deliver(Message& m) {
  struct sockaddr_in sender_addr;
  socklen_t addr_len = sizeof(sender_addr);
  ssize_t bytes_recv = recvfrom(_sockfd, m.data(), m.size(), 0,
                                reinterpret_cast<struct sockaddr*>(&sender_addr),
                                &addr_len);
  std::cerr << "[DEBUG:Deliver] Received message with seq_id: " << m.seq_id() << std::endl;
  if (bytes_recv < 0) {
    std::string err_msg = "recvfrom() failed. Error message: ";
    err_msg += strerror(errno);
    perror(err_msg.c_str());
    exit(EXIT_FAILURE);
  }

  std::cerr << "[DEBUG:Deliver] Sending ACK for seq_id: " << m.seq_id() << std::endl;

  // Send ACK back to the sender.
  Message ack(m.seq_id());
  ssize_t bytes_sent = sendto(_sockfd, ack.data(), ack.size(), 0,
                              reinterpret_cast<struct sockaddr*>(&sender_addr),
                              addr_len);
  if (bytes_sent < 0) {
    std::string err_msg = "sendto() failed. Error message: ";
    err_msg += strerror(errno);
    perror(err_msg.c_str());
    exit(EXIT_FAILURE);
  }
}
