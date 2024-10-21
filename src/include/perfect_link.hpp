#pragma once

#include <unordered_set>
#include "message.hpp"
#include "stubborn_link.hpp"

class Process;

class PerfectLink {
private:
  StubbornLink _sl;
  std::unordered_set<Message, MessageHash, MessageEqual> _sent;
  std::unordered_set<Message, MessageHash, MessageEqual> _delivered;

public:
  PerfectLink(in_addr_t addr, uint16_t port);

  int sockfd() const;
  const struct sockaddr_in& addr() const;
  const std::unordered_set<Message, MessageHash, MessageEqual>& sent() const;
  const std::unordered_set<Message, MessageHash, MessageEqual>& delivered() const;
  void send(const Message& m, sockaddr_in& q_addr);
  void deliver(Message& m);
};

