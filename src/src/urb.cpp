#include "urb.hpp"

Urb::Urb(uint64_t pid, in_addr_t addr, uint16_t port,
         const std::vector<Parser::Host>& hosts,
         EventLoop &read_event_loop, EventLoop &write_event_loop,
         ThreadPool *thread_pool,
         DeliverCallback deliver_cb)
        : _pid(pid), _addr(addr), _port(port), _hosts(hosts),
          _stop(false), _deliver_cb(std::move(deliver_cb)) {
  _pl = new PerfectLink(pid, addr, port, hosts, read_event_loop, write_event_loop,
                        [this](const Packet& pkt) {
                          this->beb_deliver(pkt);
                        });

  thread_pool->enqueue([this] {
      this->monitor_delivery();
  });
}

Urb::~Urb() {
  delete _pl;
}

void Urb::broadcast(const std::vector<Packet>& packets) {
  {
    std::lock_guard<std::mutex> lock(_pending_mutex);
    for (const auto& pkt : packets) {
      _pending.insert(pkt);
    }
  }

  beb_broadcast(packets);
}

void Urb::stop() {
  _stop.store(true);
  _pl->stop();
}

void Urb::beb_broadcast(const std::vector<Packet>& packets) {
  // Deliver packets to self.
  for (const auto& pkt : packets) {
    beb_deliver(pkt);
  }

  for (const auto& host : _hosts) {
    if (host.id != _pid) {
      _pl->send(packets, host.id);
    }
  }
}

void Urb::beb_deliver(const Packet& pkt) {
  {
    std::lock_guard<std::mutex> lock(_ack_proc_map_mutex);
    auto it = _ack_proc_map.find(pkt);
    if (it == _ack_proc_map.end()) {
      _ack_proc_map[pkt] = std::unordered_set<uint64_t>();
    }
    _ack_proc_map[pkt].insert(pkt.pid());
  }

  std::vector<Packet> packets;
  {
    std::lock_guard<std::mutex> lock(_pending_mutex);
    if (_pending.find(pkt) == _pending.end()) {
      _pending.insert(pkt);
      packets.push_back(pkt);
    }
  }
  if (!packets.empty()) {
    beb_broadcast(packets);
  }
}

bool Urb::can_deliver(const Packet& pkt) {
  std::lock_guard<std::mutex> lock(_ack_proc_map_mutex);
  auto it = _ack_proc_map.find(pkt);
  if (it == _ack_proc_map.end()) {
    return false;
  }
  return it->second.size() > _hosts.size() / 2;
}

void Urb::do_deliver(const Packet& pkt) {
  {
    std::lock_guard lock(_delivered_mutex);
    auto delivered = std::make_pair(pkt.pid(), pkt.seq_id());
    if (_delivered.find(delivered) != _delivered.end()) {
      return;
    }
    _delivered.insert(delivered);
  }

  _deliver_cb(pkt);
}

void Urb::monitor_delivery() {
  while (!_stop.load()) {
    std::vector<Packet> pending_packets;
    {
      std::lock_guard<std::mutex> lock(_pending_mutex);
      for (const auto &pending: _pending) {
        pending_packets.push_back(pending);
      }
    }

    for (const auto &pkt: pending_packets) {
      if (can_deliver(pkt)) {
        do_deliver(pkt);
      }
    }
  }
}