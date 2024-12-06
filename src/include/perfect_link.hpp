#pragma once

#include <unordered_set>
#include <unordered_map>
#include "packet.hpp"
#include "stubborn_link.hpp"
#include "parser.hpp"
#include "event_loop.hpp"
#include "config.hpp"

using delivered_t = std::pair<uint64_t, uint32_t>;
struct DeliveredHash {
    std::size_t operator()(const delivered_t & p) const {
      std::hash<uint64_t> hash_pid;
      std::hash<uint32_t> hash_seq_id;

      std::size_t seed = hash_pid(p.first);
      seed ^= hash_seq_id(p.second) + 0x9e3779b9 + (seed << 6) + (seed >> 2);

      return seed;
    }
};
struct DeliveredEqual {
    bool operator()(const delivered_t &lhs, const delivered_t &rhs) const {
      return lhs.first == rhs.first && lhs.second == rhs.second;
    }
};

class PerfectLink {
private:
//  std::mutex _delivered_mutex;
//  std::unordered_set<delivered_t, DeliveredHash, DeliveredEqual> _delivered;
  std::vector<std::mutex> _delivered_mutexes;
  std::vector<std::unordered_set<delivered_t, DeliveredHash, DeliveredEqual>> _delivered;
  DeliverCallback _deliver_cb;
  std::unordered_map<uint64_t, StubbornLink*> _sl_map;
  std::atomic<bool> _stop{false};

  void deliver_packet(Packet &&pkt);
//  void deliver_packet(const Packet& pkt);
public:
  PerfectLink(uint64_t pid, in_addr_t addr, uint16_t port,
              const std::vector<Parser::Host>& hosts,
              EventLoop &read_event_loop, EventLoop &write_event_loop,
              DeliverCallback deliver_cb);
  ~PerfectLink();

  void send(const std::vector<Packet> &packets, uint64_t peer);
  void stop();
};

