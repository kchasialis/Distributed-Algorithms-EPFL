#include <cassert>
#include <utility>
#include "perfect_link.hpp"
#include "packet.hpp"

PerfectLink::PerfectLink(uint64_t pid, in_addr_t addr, uint16_t port, bool sender,
                         const std::vector<Parser::Host>& hosts, uint64_t receiver_proc,
                         EventLoop& event_loop, DeliverCallback deliver_cb) :
                         _addr(addr), _port(port), _sender(sender) {
  {
    std::lock_guard<std::mutex> lock(_delivered_mutex);
    _deliver_cb = std::move(deliver_cb);
  }

  if (sender) {
    // Connect only to receiver.
    for (const auto& host : hosts) {
      if (host.id == receiver_proc) {
        std::cerr << "[DEBUG] PerfectLink::PerfectLink: Connecting to host " << host.id << std::endl;
        _sl_map[host.id] = new StubbornLink(pid, addr, port, host.ip, host.port,
                                            sender, event_loop, [this](const Packet& pkt) {
          this->deliver_packet(pkt);
        });
        break;
      }
    }
  } else {
    // Receiver. Connect to all hosts except ourselves.
    for (const auto& host : hosts) {
      if (host.id == pid) {
        continue;
      }
      std::cerr << "[DEBUG] PerfectLink::PerfectLink: Connecting to host " << host.id << std::endl;
      _sl_map[host.id] = new StubbornLink(pid, addr, port, host.ip, host.port, sender, event_loop,
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
    // TOOD(kostas): Try to void mutex here. Use shared memory so that each thread stores its delivered messages and in the end merge w/o duplicates and output.
    // TODO(kostas): Think if we need unpack here and check if the seq_ids contained in the message are delivered.  */
    auto p = std::make_pair(pkt.pid(), pkt.seq_id());
    {
      std::lock_guard<std::mutex> lock(_delivered_mutex);
      if (_delivered.find(p) != _delivered.end()) {
        return;
      }
      _delivered.insert(p);

      _deliver_cb(pkt);
    }
  }
}

void PerfectLink::send(uint32_t n_messages, uint64_t peer, std::ofstream &outfile) {
//  _sl->send(p, addr);
//  std::cerr << "[DEBUG] Sending to peer " << peer << std::endl;
  _sl_map[peer]->send(n_messages, outfile);
}

void PerfectLink::send_syn_packets() {
  std::cerr << "[DEBUG] Sending SYN packets to senders!" << std::endl;
  const int interval_ms = 200;
  std::unordered_map<uint64_t, bool> syn_acked;
  for (const auto& sl : _sl_map) {
    syn_acked[sl.first] = false;
  }
  while (true) {
    for (const auto& sl : _sl_map) {
      if (!syn_acked[sl.first]) {
        if (!sl.second->send_syn_packet()) {
          syn_acked[sl.first] = true;
        }
      }
    }
    bool all_acked = true;
    for (const auto& acked : syn_acked) {
      if (!acked.second) {
        all_acked = false;
        break;
      }
    }
    if (all_acked || _stop.load()) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
  }
}

void PerfectLink::stop() {
  for (auto& sl : _sl_map) {
    sl.second->stop();
  }
  _stop.store(true);
}

