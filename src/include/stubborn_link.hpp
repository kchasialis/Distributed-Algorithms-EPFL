#pragma once

#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <unordered_set>
#include "udp_socket.hpp"
#include "packet.hpp"
#include "event_loop.hpp"
#include "parser.hpp"

using DeliverCallback = std::function<void(const Packet& pkt)>;

class StubbornLink {
public:
  StubbornLink(in_addr_t addr, uint16_t port, in_addr_t paddr, uint16_t pport,
               bool sender, EventLoop &event_loop, DeliverCallback _deliver_cb);
  ~StubbornLink();

  void send(const Packet &p);

private:
  UDPSocket _socket;
  bool _sender;
//  struct sockaddr_in _local_addr{};
//  struct sockaddr_in _peer_addr{};
  EventLoop &_event_loop;
  std::unordered_set<Packet, PacketHash, PacketEqual> unacked_packets;
  // Receive callback.
  std::mutex _interval_mutex;
  std::condition_variable _resend_cv;
  DeliverCallback _deliver_cb;
//    std::mutex pid_addr_map_mutex;

  std::mutex _unacked_mutex;
  std::queue<Packet> _resend_queue;
  std::thread _resend_thread;
//    std::condition_variable _send_cv;
//    bool _stop_thread;
  std::atomic<bool> _stop_thread;

  void read_event_handler(uint32_t events);

//  void write_event_handler(uint32_t events);

  void process_packet(const Packet &pkt);

  void resend_unacked_messages();
//  void receive_ack(uint32_t seq_id);
//  void enable_epollout();
//  void disable_epollout();
};
