#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <utility>
#include <cassert>
#include "stubborn_link.hpp"

StubbornLink::StubbornLink(uint64_t pid, in_addr_t addr, uint16_t port,
                           in_addr_t paddr, uint16_t pport,
                           EventLoop& event_loop, DeliverCallback deliver_cb) :
                           _socket(addr, port), _deliver_cb(std::move(deliver_cb)),
                           _pid(pid), _stop(false), _max_budget(128), _current_budget(128),
                           _budget_replenish_amount(32), _budget_replenish_interval_ms(150),
                           _last_replenish_time(std::chrono::steady_clock::now()) {

  struct sockaddr_in peer_addr{};
  peer_addr.sin_family = AF_INET;
  peer_addr.sin_port = pport;
  peer_addr.sin_addr.s_addr = paddr;
  _socket.conn(peer_addr);

  _pport = pport;
  _peer_addr = peer_addr;

  _read_event_handler = new ReadEventHandler(&_socket,
                                             [this](const Packet& pkt) { this->process_packet(pkt); });
  _read_event_data.events = EPOLLIN;
  _read_event_data.fd = _socket.infd();
  _read_event_data.handler_obj = _read_event_handler;

  _write_event_handler = new WriteEventHandler([this] { this->send_unacked_packets(); });
  _write_event_data.events = EPOLLOUT;
  _write_event_data.fd = _socket.outfd();
  _write_event_data.handler_obj = _write_event_handler;

  event_loop.add(&_read_event_data);
  event_loop.add(&_write_event_data);
}

void StubbornLink::process_packet(const Packet& pkt) {
  switch (pkt.packet_type()) {
    case PacketType::ACK:
    {
//      std::cerr << "ACK packet received with seq_id: " << pkt.seq_id() << " from process " << pkt.pid()  << std::endl;
      bool found;
      {
        std::lock_guard<std::mutex> lock(_unacked_mutex);
//        bool print_msg = false;
//        if (_unacked_packets.size() == 1) {
//          print_msg = true;
//        }
        found = _unacked_packets.erase(pkt) > 0;
//        if (_unacked_packets.empty() && print_msg) {
//          std::cerr << "NO MORE UNACKED PACKETS FROM: " << ntohs(_pport) << std::endl;
//        }
      }

      if (found) {
//        std::cerr << "ACK received for packet with seq_id: " << pkt.seq_id() << " from " << ntohs(_pport) << " " << pkt.pid() << std::endl;
        // ACK received. We need to replenish the budget.
        adjust_budget(1);
      }
      break;
    }
    case PacketType::DATA:
    {
//      std::cerr << "Data packet received with seq_id: " << pkt.seq_id() << " from " << pkt.pid() << std::endl;
      // It is a data packet.

      // Send an ACK.
//      std::cerr << "Sending ACK for packet with seq_id: " << pkt.seq_id() << " to " << _pport << std::endl;
      _deliver_cb(pkt);

      Packet ack_pkt(pkt.pid(), PacketType::ACK, pkt.seq_id());
//      _socket.sendto_buf(ack_pkt.serialize(), _peer_addr);
      _socket.send_buf(ack_pkt.serialize());

      break;
    }
    default:
      std::cerr << "Unknown packet type!" << std::endl;
      break;
  }
}

void StubbornLink::store_packets(const std::vector<Packet> &packets) {
  {
    std::lock_guard<std::mutex> lock(_unacked_mutex);
    for (const auto &pkt: packets) {
      _unacked_packets.insert(pkt);
    }
//    std::cerr << "Process: " << _pid << " unacked packets for: " << ntohs(_pport) << " size: " << _unacked_packets.size() << std::endl;
  }
}

void StubbornLink::send_unacked_packets() {
  const int initial_interval_ms = 50;
  const int max_interval_ms = 1000;
  int timeout_interval_ms = initial_interval_ms;

  while (!_stop.load()) {
    std::vector<Packet> packets_to_send;

    {
      std::unique_lock<std::mutex> lock(_unacked_mutex);
      if (_unacked_packets.empty()) {
        return;  // Exit if there are no unacknowledged packets
      }

      // Populate packets_to_send based on the current budget
      auto it = _unacked_packets.begin();
      for (int i = 0; i < _current_budget.load() && it != _unacked_packets.end(); ++i, ++it) {
        packets_to_send.push_back(*it);
      }
    }

    // Send packets within the current budget
    size_t sent = 0;
    for (const auto& pkt : packets_to_send) {
      auto pkt_serialized = pkt.serialize();
      ssize_t nsent = _socket.send_buf(pkt_serialized);
      if (nsent == -1) {
        if (errno == EWOULDBLOCK) {
          std::cerr << "OUTPUT BUFFER BLOCKED" << std::endl;
          break;
        } else if (errno == ECONNREFUSED) {
          return;
        } else {
          perror("send failed");
          exit(EXIT_FAILURE);
        }
      } else {
        sent++;
      }
    }

    // Safely decrement the budget by the number of packets sent
    adjust_budget(-static_cast<int>(packets_to_send.size()));

    // Periodic replenishment of budget.
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - _last_replenish_time).count() >= _budget_replenish_interval_ms) {
      adjust_budget(_budget_replenish_amount);
      _last_replenish_time = now;
    }

    // Adjust timeout based on how many packets were sent
    if (sent == packets_to_send.size()) {
      timeout_interval_ms = initial_interval_ms;
    } else {
      timeout_interval_ms = std::min(backoff_interval(timeout_interval_ms), max_interval_ms);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(timeout_interval_ms));
  }
}

void StubbornLink::send(const std::vector<Packet> &packets) {
  store_packets(packets);
}

void StubbornLink::stop() {
  _stop.store(true);
}

int StubbornLink::backoff_interval(int timeout) {
  std::uniform_int_distribution<int> distribution(timeout, 2 * timeout);
  return distribution(_random_engine);
}

void StubbornLink::adjust_budget(int delta) {
  int current = _current_budget.load(std::memory_order_relaxed);
  int max_budget = _max_budget.load(std::memory_order_relaxed);

  while (true) {
    int new_budget = current + delta;
    if (new_budget < 0) {
      new_budget = 0;
    } else if (new_budget > max_budget) {
      new_budget = max_budget;
    }

    if (_current_budget.compare_exchange_weak(current, new_budget, std::memory_order_relaxed)) {
      break;
    }
  }
}