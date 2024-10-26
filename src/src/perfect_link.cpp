#include <cassert>
#include <utility>
#include "perfect_link.hpp"
#include "packet.hpp"

PerfectLink::PerfectLink(in_addr_t addr, uint16_t port, bool sender,
                         const std::vector<Parser::Host>& hosts,
                         EventLoop& event_loop, DeliverCallback deliver_cb) :
                         _addr(addr), _port(port), _sender(sender) {
  _deliver_cb = std::move(deliver_cb);
  for (const auto& host : hosts) {
    // Connect to all hosts.
//    std::cerr << "[DEBUG] PerfectLink::PerfectLink: Connecting to host " << host.id << std::endl;
    _sl_map[host.id] = new StubbornLink(addr, port, host.ip, host.port, sender, event_loop,
                                        [this](const std::vector<uint8_t>& data) {
    this->deliver_packet(data);
    });
  }
}

PerfectLink::~PerfectLink() {
  for (auto& sl : _sl_map) {
    delete sl.second;
  }
}

void PerfectLink::deliver_packet(const std::vector<uint8_t>& data) {
  if (!_sender) {
    in_addr addr{};
    addr.s_addr = _addr;
    Packet packet;
    packet.deserialize(data);
    auto p = std::make_pair(packet.pid(), packet.seq_id());
    if (_delivered.find(p) != _delivered.end()) {
      return;
    }
    _delivered.insert(p);

    _deliver_cb(data);
  }
}

void PerfectLink::send(const Packet& p, uint64_t peer) {
//  _sl->send(p, addr);
  std::cerr << "[DEBUG] Sending to peer " << peer << std::endl;
  _sl_map[peer]->send(p);
}

//bool PerfectLink::all_acked(uint64_t peer) {
//  return _sl_map[peer]->all_acked();
//}
