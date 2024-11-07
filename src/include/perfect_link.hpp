#pragma once

#include <unordered_set>
#include <unordered_map>
#include "packet.hpp"
#include "stubborn_link.hpp"
#include "parser.hpp"
#include "event_loop.hpp"

class PerfectLink {
private:
  using delivered_t = std::pair<uint64_t, uint32_t>;
  struct PairHash {
      std::size_t operator()(const delivered_t & p) const {
        std::size_t h1 = std::hash<uint64_t>{}(p.first);
        std::size_t h2 = std::hash<uint32_t>{}(p.second);

        // Combine the two hashes using XOR and bit shifting
        return h1 ^ (h2 << 1);
      }
  };

  in_addr_t _addr;
  uint16_t _port;
  bool _sender;
  std::mutex _delivered_mutex;
  std::unordered_set<delivered_t, PairHash> _delivered;
  DeliverCallback _deliver_cb;
  std::unordered_map<uint64_t, StubbornLink*> _sl_map;
  std::atomic<bool> _stop{false};

  void deliver_packet(const Packet& pkt);
public:
  PerfectLink(uint64_t pid, in_addr_t addr, uint16_t port, bool sender,
              const std::vector<Parser::Host>& hosts, uint64_t receiver_proc,
              EventLoop& event_loop, DeliverCallback deliver_cb);
  ~PerfectLink();

  void send(uint32_t n_messages, uint64_t peer, std::ofstream &outfile);
  void send_syn_packets();
  void stop();
};

