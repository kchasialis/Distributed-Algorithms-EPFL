#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "process.hpp"
#include "perfect_link.hpp"

Process::Process(uint64_t id, in_addr_t addr, uint16_t port, bool sender) :
        _pid(getpid()), _id(id), _link(std::make_unique<PerfectLink>(addr, port, sender)),
        _sender(sender) {}

int Process::pid() const {
  return _pid;
}

uint64_t Process::id() const {
  return _id;
}

bool Process::sender() const {
  return _sender;
}

const PerfectLink& Process::link() const {
  return *_link;
}

bool Process::operator==(const Process& other) const {
  auto this_addr = this->_link->addr();
  auto other_addr = other._link->addr();

  if (this_addr.sin_addr.s_addr != other_addr.sin_addr.s_addr || this_addr.sin_port != other_addr.sin_port) {
      return false;
  }

  return this->_pid == other._pid && this->_sender == other._sender;
}
