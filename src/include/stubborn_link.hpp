#pragma once

#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <set>
#include <unordered_set>
#include <condition_variable>
#include <random>
#include "udp_socket.hpp"
#include "packet.hpp"
#include "event_loop.hpp"
#include "parser.hpp"
#include "event_handler.hpp"

//using DeliverCallback = std::function<void(const Packet& pkt)>;
using DeliverCallback = std::function<void(Packet &&pkt)>;

class StubbornLink {
public:
  StubbornLink(uint64_t pid, in_addr_t addr, uint16_t port,
               in_addr_t paddr, uint16_t pport,
               EventLoop &read_event_loop, EventLoop &write_event_loop,
               DeliverCallback _deliver_cb);
  ~StubbornLink();

  void send(const std::vector<Packet> &packets);
  void stop();
private:
  // We might relay messages from other processes, we want to check for the pair seq_id, pid
  struct UnAckedPacketEqual {
      bool operator()(const Packet& lhs, const Packet& rhs) const {
        return lhs.seq_id() == rhs.seq_id() && lhs.pid() == rhs.pid();
      }
  };

  struct UnAckedPacketHash {
      std::size_t operator()(const Packet& pkt) const {
        std::hash<uint32_t> hash_seq_id;
        std::hash<uint64_t> hash_pid;

        std::size_t seed = hash_seq_id(pkt.seq_id());
        seed ^= hash_pid(pkt.pid()) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        return seed;
      }
  };

  UDPSocket _socket;
  std::unordered_set<Packet, UnAckedPacketHash, UnAckedPacketEqual> _unacked_packets;
  std::condition_variable _resend_cv;
  DeliverCallback _deliver_cb;
  uint64_t _pid;
  struct sockaddr_in _peer_addr;
  uint16_t _pport;
  ReadEventHandler *_read_event_handler;
  EventData _read_event_data{};
  WriteEventHandler *_write_event_handler;
  EventData _write_event_data{};

  std::mutex _unacked_mutex;
  std::thread _resend_thread;
  std::atomic<bool> _stop;
  std::default_random_engine _random_engine{std::random_device{}()};

  std::atomic<int> _current_budget;
  static constexpr int _max_budget = 32;
  static constexpr int _budget_replenish_amount = 16;
  static constexpr int _budget_replenish_interval_ms = 100;
  std::chrono::steady_clock::time_point _last_replenish_time;

  void send_unacked_packets();
  void process_packet(Packet &&pkt);
  void store_packets(const std::vector<Packet> &packets);
  int backoff_interval(int timeout);
  void adjust_budget(int amount);
};
