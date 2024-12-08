#include <cassert>
#include <utility>
#include "perfect_link.hpp"
#include "packet.hpp"

PerfectLink::PerfectLink(uint64_t pid, in_addr_t addr, uint16_t port,
                         const std::vector<Parser::Host>& hosts,
                         EventLoop &read_event_loop, EventLoop &write_event_loop,
                         DeliverCallback deliver_cb) :
        _delivered_mutexes(hosts.size()), _delivered(hosts.size()),
        _deliver_cb(std::move(deliver_cb)) {

  for (const auto& host : hosts) {
    if (host.id == pid) {
      continue;
    }
    _sl_map[host.id] = new StubbornLink(pid, addr, port, host.ip, host.port,
                                        read_event_loop, write_event_loop,
                                        [this](Packet &&pkt) {
                                            this->deliver_packet(std::move(pkt));
                                        });
  }
}

PerfectLink::~PerfectLink() {
  for (auto& sl : _sl_map) {
    delete sl.second;
  }
}

void PerfectLink::deliver_packet(Packet &&pkt) {
  auto p = std::make_pair(pkt.pid(), pkt.seq_id());
  {
    size_t index = pkt.pid() - 1;
    std::lock_guard<std::mutex> lock(_delivered_mutexes[index]);
    auto& delivered_set = _delivered[index];
    if (delivered_set.find(p) != delivered_set.end()) {
      return;
    }

    delivered_set.insert(p);
  }

  _deliver_cb(std::move(pkt));
}

void PerfectLink::send(const std::vector<Packet> &packets, uint64_t peer) {
  _sl_map[peer]->send(packets);
}

void PerfectLink::stop() {
  for (auto& sl : _sl_map) {
    sl.second->stop();
  }
}

