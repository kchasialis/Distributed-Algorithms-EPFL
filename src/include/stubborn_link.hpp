#pragma once

#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <unordered_set>
#include <condition_variable>
#include <random>
#include "udp_socket.hpp"
#include "packet.hpp"
#include "event_loop.hpp"
#include "parser.hpp"

using DeliverCallback = std::function<void(const Packet& pkt)>;

class StubbornLink {
public:
  StubbornLink(uint64_t pid, in_addr_t addr, uint16_t port,
               in_addr_t paddr, uint16_t pport,
               bool sender, EventLoop &event_loop, DeliverCallback _deliver_cb);

  void send(uint32_t n_messages, std::ofstream &outfile, std::mutex &outfile_mutex);
  bool send_syn_packet();
  void stop();
private:
  UDPSocket _socket;
  bool _sender;
  EventLoop &_event_loop;
  std::unordered_set<Packet, PacketHash, PacketEqual> unacked_packets;
  std::mutex _interval_mutex;
  std::condition_variable _resend_cv;
  DeliverCallback _deliver_cb;
  uint64_t _pid;

  std::condition_variable _syn_received_cv;
  std::mutex _syn_mutex;
  std::atomic<bool> _syn_received{false};
  std::mutex _unacked_mutex;
  std::queue<Packet> _resend_queue;
  std::thread _resend_thread;
  std::thread _syn_resend_thread;
  std::atomic<bool> _stop;
  std::atomic<bool> _syn_ack_received{false};
  std::atomic<bool> _ack_received;
  std::default_random_engine _random_engine{std::random_device{}()};

  void read_event_handler(uint32_t events);

  void process_packet(const Packet &pkt);

//  void resend_syn_packet();
//
//  void resend_unacked_packets();
  int backoff_interval(int timeout);
};
