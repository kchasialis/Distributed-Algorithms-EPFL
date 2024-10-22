#pragma once

#include <unordered_set>
#include <unordered_map>
#include "message.hpp"
#include "stubborn_link.hpp"
#include "parser.hpp"

class Process;

struct DeliverMessage {
    uint64_t pid;
    Message msg;
};

struct DeliverMessageHash {
    std::size_t operator()(const DeliverMessage& dm) const {
      std::size_t h1 = std::hash<uint64_t>()(dm.pid);
      std::size_t h2 = MessageHash()(dm.msg);

      return h1 ^ (h2 << 1);
    }
};

struct DeliverMessageEqual {
    bool operator()(const DeliverMessage& lhs, const DeliverMessage& rhs) const {
      return lhs.pid == rhs.pid && lhs.msg == rhs.msg;
    }
};

struct SockAddrInHash {
    std::size_t operator()(const sockaddr_in& addr) const {
      std::size_t h1 = std::hash<uint32_t>()(addr.sin_addr.s_addr);
      std::size_t h2 = std::hash<uint16_t>()(addr.sin_port);
      return h1 ^ (h2 << 1);
    }
};

struct SockAddrInEqual {
    bool operator()(const sockaddr_in& lhs, const sockaddr_in& rhs) const {
      return lhs.sin_addr.s_addr == rhs.sin_addr.s_addr && lhs.sin_port == rhs.sin_port;
    }
};

class PerfectLink {
private:
  StubbornLink _sl;
  std::unordered_map<struct sockaddr_in, uint64_t, SockAddrInHash, SockAddrInEqual> addr_pid_map;
  std::vector<Message> _sent;
  std::unordered_set<DeliverMessage, DeliverMessageHash, DeliverMessageEqual> _delivered;

public:
  PerfectLink(in_addr_t addr, uint16_t port, const std::vector<Parser::Host>& hosts);

  int sockfd() const;
  const struct sockaddr_in& addr() const;
  const std::vector<Message>& sent() const;
  const std::unordered_set<DeliverMessage, DeliverMessageHash, DeliverMessageEqual>& delivered() const;
  void send(const Message& m, sockaddr_in& q_addr);
  void deliver();
};

