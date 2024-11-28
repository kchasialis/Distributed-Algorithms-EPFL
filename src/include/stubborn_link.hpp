#pragma once

#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <set>
#include <condition_variable>
#include <random>
#include "udp_socket.hpp"
#include "packet.hpp"
#include "event_loop.hpp"
#include "parser.hpp"
#include "read_event_handler.hpp"

using DeliverCallback = std::function<void(const Packet& pkt)>;

//constexpr int sliding_window_size = 32;

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
  std::set<Packet, PacketLess> unacked_packets;
  std::condition_variable _resend_cv;
  DeliverCallback _deliver_cb;
  uint64_t _pid;
  ReadEventHandler *_read_event_handler;
  EventData _read_event_data{};


  std::condition_variable _syn_received_cv;
  std::mutex _syn_mutex;
  std::atomic<bool> _syn_received{false};
  std::mutex _unacked_mutex;
  std::thread _resend_thread;
  std::thread _syn_resend_thread;
  std::atomic<bool> _stop;
  std::atomic<bool> _syn_ack_received{false};
  std::default_random_engine _random_engine{std::random_device{}()};

  void send_unacked_messages();
  void process_packet(const Packet &pkt);
  void store_and_output_messages(uint32_t n_messages, std::ofstream &outfile, std::mutex &outfile_mutex);
  int backoff_interval(int timeout);
};
