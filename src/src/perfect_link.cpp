#include <cassert>
#include <utility>
#include "perfect_link.hpp"
#include "packet.hpp"

PerfectLink::PerfectLink(size_t n_messages, in_addr_t addr, uint16_t port, bool sender,
                         const std::vector<Parser::Host>& hosts, uint64_t receiver_proc,
                         EventLoop& event_loop, DeliverCallback deliver_cb) :
                         _addr(addr), _port(port), _sender(sender), _n_messages(n_messages) {
  _deliver_cb = std::move(deliver_cb);

  if (sender) {
    // Connect only to receiver.
    for (const auto& host : hosts) {
      if (host.id == receiver_proc) {
        std::cerr << "[DEBUG] PerfectLink::PerfectLink: Connecting to host " << host.id << std::endl;
        _sl_map[host.id] = new StubbornLink(addr, port, host.ip, host.port, sender, event_loop,
                                            [this](const Packet& pkt) {
          this->deliver_packet(pkt);
        });
        break;
      }
    }
  } else {
    // Receiver. Connect to all hosts.
    for (const auto& host : hosts) {
      std::cerr << "[DEBUG] PerfectLink::PerfectLink: Connecting to host " << host.id << std::endl;
      _sl_map[host.id] = new StubbornLink(addr, port, host.ip, host.port, sender, event_loop,
                                          [this](const Packet& pkt) {
                                              this->deliver_packet(pkt);
                                          });
    }
  }
}

PerfectLink::~PerfectLink() {
  for (auto& sl : _sl_map) {
    delete sl.second;
  }
}

void PerfectLink::deliver_packet(const Packet& pkt) {
  if (!_sender) {
    auto p = std::make_pair(pkt.pid(), pkt.seq_id());
    {
      std::lock_guard lock(_delivered_mutex);
      if (_delivered.find(p) != _delivered.end()) {
        return;
      }
      _delivered.insert(p);

      if (_delivered.size() == _n_messages) {
        std::cerr << "[DEBUG] All messages delivered" << std::endl;
      }

      _deliver_cb(pkt);
    }
  }
}

void PerfectLink::send(const Packet& p, uint64_t peer) {
//  _sl->send(p, addr);
//  std::cerr << "[DEBUG] Sending to peer " << peer << std::endl;
  _sl_map[peer]->send(p);
}

//bool PerfectLink::all_acked(uint64_t peer) {
//  return _sl_map[peer]->all_acked();
//}
