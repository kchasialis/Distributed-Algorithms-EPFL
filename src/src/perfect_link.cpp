#include <cassert>
#include "perfect_link.hpp"

PerfectLink::PerfectLink(in_addr_t addr, uint16_t port,
                         const std::vector<Parser::Host>& hosts) :
        _sl(addr, port), _sent(), _delivered() {
  for (const auto& host : hosts) {
    struct sockaddr_in host_addr{};
    host_addr.sin_family = AF_INET;
    host_addr.sin_port = host.port;
    host_addr.sin_addr.s_addr = host.ip;
    addr_pid_map[host_addr] = host.id;
  }
}

int PerfectLink::sockfd() const {
  return _sl.sockfd();
}

const struct sockaddr_in& PerfectLink::addr() const {
  return _sl.addr();
}

const std::vector<Message>& PerfectLink::sent() const {
  return _sent;
}

const std::unordered_set<DeliverMessage, DeliverMessageHash, DeliverMessageEqual>& PerfectLink::delivered() const {
  return _delivered;
}

void PerfectLink::send(const Message& m, sockaddr_in& q_addr) {
  _sl.send(m, q_addr);
  _sent.push_back(m);
}

void PerfectLink::deliver() {
  Message msg{};
  auto sender_addr = _sl.deliver(msg);

  assert(addr_pid_map.find(sender_addr) != addr_pid_map.end());

  DeliverMessage dm{addr_pid_map[sender_addr], msg};

  std::cerr << "[DEBUG-PL] Delivering message with id: " << msg.seq_id() << " from process: " << dm.pid << std::endl;

  if (_delivered.find(dm) != _delivered.end()) {
    return;
  }
  _delivered.insert(dm);
}
