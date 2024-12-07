#include "urb.hpp"

Urb::Urb(uint64_t pid, in_addr_t addr, uint16_t port,
         const std::vector<Parser::Host>& hosts,
         EventLoop &read_event_loop, EventLoop &write_event_loop,
         ThreadPool *thread_pool,
         DeliverCallback deliver_cb)
        : _pid(pid), _addr(addr), _port(port), _hosts(hosts),
          _pending_sets(hosts.size()), _pending_mutexes(hosts.size()),
          _stop(false), _deliver_cb(std::move(deliver_cb)) {
  _pl = new PerfectLink(pid, addr, port, hosts, read_event_loop, write_event_loop,
                        [this](Packet &&pkt) {
                            this->beb_deliver(std::move(pkt));
                        });

  for (uint32_t i = 0; i < monitor_delivery_workers; i++) {
    thread_pool->enqueue([this, i] {
        this->monitor_delivery(i, monitor_delivery_workers);
    });
  }
}

Urb::~Urb() {
  delete _pl;
}

void Urb::stop() {
  _stop.store(true);
  _pl->stop();
}

void Urb::beb_broadcast(std::vector<Packet>& packets) {
  for (const auto& host : _hosts) {
    if (host.id != _pid) {
      _pl->send(packets, host.id);
    }
  }

  // Deliver packets to self.
  for (auto& pkt : packets) {
    beb_deliver(std::move(pkt));
  }
}

void Urb::do_deliver(Packet &&pkt) {
  _deliver_cb(std::move(pkt));
}

void Urb::broadcast(std::vector<Packet>& packets) {
  std::vector<std::vector<Packet>> partitioned(_pending_sets.size());

  for (auto& pkt : packets) {
    size_t index = pkt.pid() - 1;
    partitioned[index].push_back(pkt);
  }

  for (size_t index = 0; index < partitioned.size(); ++index) {
    if (!partitioned[index].empty()) {
      std::lock_guard<std::mutex> lock(_pending_mutexes[index]);
      for (auto& pkt : partitioned[index]) {
        _pending_sets[index].insert(pkt);
      }
    }
  }

  beb_broadcast(packets);
}

//void Urb::broadcast(std::vector<Packet>& packets) {
//  for (const auto& pkt : packets) {
//    size_t index = pkt.pid() - 1;
//    std::lock_guard<std::mutex> lock(_pending_mutexes[index]);
//    _pending_sets[index].insert(pkt);
//  }
//
//  beb_broadcast(packets);
//}

void Urb::beb_deliver(Packet&& pkt) {
  size_t index = pkt.pid() - 1;

  auto key = pkt.seq_id();
  {
    std::lock_guard<std::mutex> lock(_ack_proc_map_mutex);
    auto it = _ack_proc_map.find(key);
    if (it == _ack_proc_map.end()) {
      _ack_proc_map[key] = std::unordered_set<uint64_t>();
    }
    _ack_proc_map[key].insert(pkt.pid());
  }

  std::vector<Packet> packets;
  {
    std::lock_guard<std::mutex> lock(_pending_mutexes[index]);
    auto& pending_set = _pending_sets[index];
    if (pending_set.find(pkt) == pending_set.end()) {
      packets.push_back(std::move(pkt));
      pending_set.insert(packets.back());
    }
  }

  if (!packets.empty()) {
    beb_broadcast(packets);
  }
}

bool Urb::can_deliver(const Packet& pkt) {
  std::lock_guard<std::mutex> lock(_ack_proc_map_mutex);

  auto key = pkt.seq_id();
  auto it = _ack_proc_map.find(key);
  if (it == _ack_proc_map.end()) {
    return false;
  }
  return it->second.size() > _hosts.size() / 2;
}

void Urb::monitor_delivery(size_t thread_id, size_t n_threads) {
  while (!_stop.load()) {
    std::vector<Packet> pending_packets;

    // Each thread processes a subset of pending_sets
    for (size_t i = thread_id; i < _pending_sets.size(); i += n_threads) {
      {
        std::lock_guard<std::mutex> lock(_pending_mutexes[i]);
        for (const auto& pending : _pending_sets[i]) {
          pending_packets.push_back(pending);
        }
      }
    }

    for (auto& pkt : pending_packets) {
      if (can_deliver(pkt)) {
        do_deliver(std::move(pkt));
      }
    }
  }
}
