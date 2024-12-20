#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <utility>
#include <cassert>
#include "stubborn_link.hpp"

StubbornLink::StubbornLink(uint64_t pid, in_addr_t addr, uint16_t port,
                           in_addr_t paddr, uint16_t pport,
                           EventLoop &read_event_loop, EventLoop &write_event_loop,
                           DeliverCallback deliver_cb) :
                           _socket(addr, port), _deliver_cb(std::move(deliver_cb)),
                           _pid(pid), _stop(false), _current_budget(_max_budget),
                           _last_replenish_time(std::chrono::steady_clock::now()) {

  struct sockaddr_in peer_addr{};
  peer_addr.sin_family = AF_INET;
  peer_addr.sin_port = pport;
  peer_addr.sin_addr.s_addr = paddr;
  _socket.conn(peer_addr);

  _pport = pport;
  _peer_addr = peer_addr;

  _read_event_handler = new ReadEventHandler(&_socket,
                                             [this](Packet &&pkt) { this->process_packet(std::move(pkt)); });
  _read_event_data.events = EPOLLIN;
  _read_event_data.fd = _socket.infd();
  _read_event_data.handler_obj = _read_event_handler;

  _write_event_handler = new WriteEventHandler([this] { this->send_unacked_packets(); });
  _write_event_data.events = EPOLLOUT;
  _write_event_data.fd = _socket.outfd();
  _write_event_data.handler_obj = _write_event_handler;

  read_event_loop.add(&_read_event_data);
  write_event_loop.add(&_write_event_data);
}

StubbornLink::~StubbornLink() {
  delete _read_event_handler;
  delete _write_event_handler;
}

void StubbornLink::process_packet(Packet &&pkt) {
  switch (pkt.type()) {
    case PacketType::ACK:
    {
      bool found;
      {
        std::lock_guard<std::mutex> lock(_unacked_mutex);
        found = _unacked_packets.erase(pkt) > 0;
      }

      if (found) {
        // ACK received. We need to replenish the budget.
        adjust_budget(1);
      }
      break;
    }
    case PacketType::DATA:
    {
      // Send an ACK.
      Packet ack_pkt(pkt.pid(), PacketType::ACK, pkt.seq_id());
      std::vector<uint8_t> buffer;
      ack_pkt.serialize(buffer);
      _socket.send_buf(buffer);

      _deliver_cb(std::move(pkt));
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
    for (const auto &pkt : packets) {
      _unacked_packets.insert(pkt);
    }
  }
}

void StubbornLink::store_packet(const Packet &packet) {
  {
    std::lock_guard<std::mutex> lock(_unacked_mutex);
    _unacked_packets.insert(packet);
  }
}

void StubbornLink::send_unacked_packets() {
  const int initial_interval_ms = 10;
  const int max_interval_ms = 1000;
  int timeout_interval_ms = initial_interval_ms;

  while (!_stop.load()) {
    std::vector<Packet> packets_to_send;
    packets_to_send.reserve(_max_budget);

    {
      std::unique_lock<std::mutex> lock(_unacked_mutex);
      if (_unacked_packets.empty()) {
        return;  // Exit if there are no unacknowledged packets
      }

      // Populate packets_to_send based on the current budget
      auto it = _unacked_packets.begin();
      int current_budget = _current_budget.load();
      for (int i = 0; i < current_budget && it != _unacked_packets.end(); ++i, ++it) {
        packets_to_send.push_back(*it);
      }
    }

    // Send packets within the current budget
    size_t sent = 0;
    for (const auto& pkt : packets_to_send) {
      std::vector<uint8_t> buffer;
      pkt.serialize(buffer);
      ssize_t nsent = _socket.send_buf(buffer);
      if (nsent == -1) {
        if (errno == EWOULDBLOCK) {
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
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - _last_replenish_time).count();
    if (elapsed >= _budget_replenish_interval_ms) {
      adjust_budget(_budget_replenish_amount);
      _last_replenish_time = now;
    }

    // Adjust timeout based on how many packets were sent successfully
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

void StubbornLink::send(const Packet &packet) {
  store_packet(packet);
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

  while (true) {
    int new_budget = current + delta;
    if (new_budget < 0) {
      new_budget = 0;
    } else if (new_budget > _max_budget) {
      new_budget = _max_budget;
    }

    if (_current_budget.compare_exchange_weak(current, new_budget, std::memory_order_relaxed)) {
      break;
    }
  }
}
