#include <cassert>
#include <utility>
#include "perfect_link.hpp"
#include "packet.hpp"

PerfectLink::PerfectLink(uint64_t pid, in_addr_t addr, uint16_t port,
                         const std::vector<Parser::Host>& hosts,
                         EventLoop &read_event_loop, EventLoop &write_event_loop,
                         DeliverCallback deliver_cb) {
  {
    std::lock_guard<std::mutex> lock(_delivered_mutex);
    _deliver_cb = std::move(deliver_cb);
  }

  for (const auto& host : hosts) {
    if (host.id == pid) {
      continue;
    }
    _sl_map[host.id] = new StubbornLink(pid, addr, port, host.ip, host.port,
                                        read_event_loop, write_event_loop,
                                        [this](const Packet& pkt) {
                                            this->deliver_packet(pkt);
                                        });
  }
}

PerfectLink::~PerfectLink() {
  for (auto& sl : _sl_map) {
    delete sl.second;
  }
}

void PerfectLink::deliver_packet(const Packet& pkt) {
  auto p = std::make_pair(pkt.pid(), pkt.seq_id());
  {
    std::lock_guard<std::mutex> lock(_delivered_mutex);
    if (_delivered.find(p) != _delivered.end()) {
      return;
    }
    _delivered.insert(p);
  }

  _deliver_cb(pkt);
}

void PerfectLink::send(const std::vector<Packet> &packets, uint64_t peer) {
  _sl_map[peer]->send(packets);
}

void PerfectLink::stop() {
  for (auto& sl : _sl_map) {
    sl.second->stop();
  }
  _stop.store(true);
}

