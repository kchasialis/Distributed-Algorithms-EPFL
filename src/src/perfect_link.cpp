#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include "perfect_link.hpp"
#include "process.hpp"

PerfectLink::PerfectLink(in_addr_t addr, uint16_t port) :
        _sl(addr, port), _sent(), _delivered() {}

int PerfectLink::sockfd() const {
  return _sl.sockfd();
}

const struct sockaddr_in& PerfectLink::addr() const {
  return _sl.addr();
}

const std::unordered_set<Message, MessageHash, MessageEqual>& PerfectLink::sent() const {
  return _sent;
}

const std::unordered_set<Message, MessageHash, MessageEqual>& PerfectLink::delivered() const {
  return _delivered;
}

void PerfectLink::send(const Message& m, sockaddr_in& q_addr) {
  _sl.send(m, q_addr);
  _sent.insert(m);
}

void PerfectLink::deliver(Message& m) {
  _sl.deliver(m);

  if (_delivered.find(m) != _delivered.end()) {
    return;
  }
  _delivered.insert(m);
}