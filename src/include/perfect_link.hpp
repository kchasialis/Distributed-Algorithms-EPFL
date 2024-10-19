#pragma once

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unordered_set>
#include "message.hpp"

class Process;

class PerfectLink {
private:
  int _sockfd;
  struct sockaddr_in _addr{};
  std::unordered_set<Message, MessageHash, MessageEqual> _sent;
  std::unordered_set<Message, MessageHash, MessageEqual> _delivered;

public:
  PerfectLink(in_addr_t addr, uint16_t port, bool sender);
  virtual ~PerfectLink();

  int sockfd() const;
  const struct sockaddr_in& addr() const;
  const std::unordered_set<Message, MessageHash, MessageEqual>& sent() const;
  const std::unordered_set<Message, MessageHash, MessageEqual>& delivered() const;
  void send(const Message& m, const Process& q);
  void receive(Message& m, const Process& p);
};

