#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <utility>
#include <cassert>
//#include <random>
#include "stubborn_link.hpp"

StubbornLink::StubbornLink(uint64_t pid, in_addr_t addr, uint16_t port,
                           in_addr_t paddr, uint16_t pport,
                           bool sender, EventLoop& event_loop, DeliverCallback deliver_cb) :
                           _socket(addr, port), _sender(sender), _event_loop(event_loop),
                           _deliver_cb(std::move(deliver_cb)), _pid(pid), _stop(false),
                           _ack_received(false) {

  struct sockaddr_in peer_addr{};
  peer_addr.sin_family = AF_INET;
  peer_addr.sin_port = pport;
  peer_addr.sin_addr.s_addr = paddr;
  _socket.conn(peer_addr);

  event_loop.add(_socket.infd(), EPOLLIN, [this](uint32_t events) {
      read_event_handler(events);
  });
//  _socket.set_blocking_output(true);

//  if (_sender) {
//    _resend_thread = std::thread(&StubbornLink::resend_unacked_packets, this);
//  } else {
//    _resend_thread = std::thread(&StubbornLink::resend_syn_packet, this);
//  }
//  if (!_sender) {
//    _resend_thread = std::thread(&StubbornLink::resend_syn_packet, this);
//  }
}

//  _stop_thread.store(true);
//  if (_sender) {
//    // Notify the resend thread to wake up early and stop
//    _resend_cv.notify_all();
//  }
//
//  if (_resend_thread.joinable()) {
//    _resend_thread.join();
//  }
//  std::cerr << "[DEBUG] Destroying stubborn link!" << std::endl;
//  _stop_thread.store(true);
//  if (!_sender) {
//    if (_resend_thread.joinable()) {
//      _resend_thread.join();
//    }
//  }

void StubbornLink::read_event_handler(uint32_t events) {
  if (events & EPOLLIN) {
    // Data is available to read.
    std::vector<uint8_t> buffer(RECV_BUF_SIZE, 0);
    while (true) {
      buffer.resize(RECV_BUF_SIZE);
      ssize_t nrecv = _socket.recv_buf(buffer);
      if (nrecv == -1) {
        if (errno == EWOULDBLOCK) {
          // No more data to read.
          break;
        }
        std::string err_msg = "recv() failed. Error message: ";
        err_msg += strerror(errno);
        perror(err_msg.c_str());
        exit(EXIT_FAILURE);
      }
      if (nrecv == 0) {
        continue;
      }
      // Process the received data.
      Packet pkt;
      buffer.resize(static_cast<size_t>(nrecv));
      pkt.deserialize(buffer);
      process_packet(pkt);
    }
    _event_loop.rearm(_socket.infd(), EPOLLIN);
  }
}

void StubbornLink::process_packet(const Packet& pkt) {
  switch (pkt.packet_type()) {
    case PacketType::SYN:
    {
      std::cerr << "[DEBUG] Received SYN packet from receiver!" << std::endl;
      assert(_sender);
      // Notify that SYN has been received
      {
        std::lock_guard<std::mutex> lock(_syn_mutex);
        _syn_received.store(true);
      }
      _syn_received_cv.notify_all();
      // Send a SYN_ACK.
      std::cerr << "[DEBUG] Sending SYNACK packet to receiver!" << std::endl;
      assert(pkt.seq_id() == 0);
      Packet ack_pkt(_pid, PacketType::ACK, pkt.seq_id());
      _socket.send_buf(ack_pkt.serialize());
      break;
    }
    case PacketType::ACK:
    {
      if (pkt.seq_id() == 0) {
        std::cerr << "[DEBUG] Received SYNACK packet from sender!" << std::endl;
        _syn_ack_received.store(true);
      } else {
        std::lock_guard<std::mutex> lock(_unacked_mutex);
        unacked_packets.erase(pkt);
        _ack_received.store(true);
      }
      break;
    }
    case PacketType::DATA:
    {
      // It is a data packet.
      // Send an ACK.
      Packet ack_pkt(pkt.pid(), PacketType::ACK, pkt.seq_id());
      _socket.send_buf(ack_pkt.serialize());

      // Deliver the data packet.
      _deliver_cb(pkt);
      break;
    }
    default:
      std::cerr << "Unknown packet type: " << static_cast<int>(pkt.packet_type()) << std::endl;
      break;
  }
}

void StubbornLink::send(uint32_t n_messages, std::ofstream &outfile) {
//  std::lock_guard<std::mutex> lock(_unacked_mutex);
//  unacked_packets.insert(pkt);
  {
    std::lock_guard<std::mutex> lock(_unacked_mutex);
    uint32_t current_seq_id = 1;
    for (uint32_t i = 0; i < n_messages; i += 8) {
      uint32_t packet_size = std::min(8U, n_messages - i);
      std::vector<uint8_t> data(packet_size * sizeof(uint32_t));
      for (uint32_t j = 0; j < packet_size; j++) {
        std::memcpy(data.data() + j * sizeof(uint32_t), &current_seq_id,
                    sizeof(uint32_t));
        outfile << "b " << current_seq_id << "\n";
        current_seq_id++;
      }

      Packet p(_pid, PacketType::DATA, (i / 8) + 1, data);
      unacked_packets.insert(p);
    }
  }

  const int initial_interval_ms = 100;
  const int max_interval_ms = 5000;

  int current_interval_ms = initial_interval_ms;
  uint32_t initial_window_size = 1000;
  uint32_t window_size = initial_window_size;

  std::cerr << "[DEBUG] Waiting for SYN packet from receiver..." << std::endl;
  {
    std::unique_lock<std::mutex> syn_lock(_syn_mutex);
    _syn_received_cv.wait(syn_lock, [this] { return _syn_received.load(); });
  }
  std::cerr << "[DEBUG] Unblocked! Sending packets..." << std::endl;

  while (true) {
//    std::unique_lock<std::mutex> interval_lock(_interval_mutex);
//     Wait for either the timeout or for a signal to stop the thread
//    _resend_cv.wait_for(interval_lock, std::chrono::milliseconds(current_interval_ms), [this] {
//        return _stop_thread.load();
//    });
    std::this_thread::sleep_for(std::chrono::milliseconds(current_interval_ms));

    if (_stop.load()) {
      break;
    }

    std::unique_lock<std::mutex> lock(_unacked_mutex);
    if (unacked_packets.empty()) {
      break;
    }

    uint32_t packets_sent = 0;
    for (const auto& pkt : unacked_packets) {
      if (packets_sent >= window_size) {
        window_size = initial_window_size;
        current_interval_ms = std::min(backoff_interval(current_interval_ms), max_interval_ms);
        break;
      }

      auto pkt_serialized = pkt.serialize();

      ssize_t nsent = _socket.send_buf(pkt_serialized);
      if (nsent == -1) {
        if (errno == ECONNREFUSED) {
          current_interval_ms = std::min(backoff_interval(current_interval_ms), max_interval_ms);
        } else if (errno == EWOULDBLOCK) {
          // Buffer is full. Reduce window size.
          current_interval_ms = std::min(backoff_interval(current_interval_ms), max_interval_ms);
          window_size /= 2;
          break;
        } else {
          perror("send failed");
        }
      } else {
        packets_sent++;
      }
    }
    lock.unlock();
  }
}

bool StubbornLink::send_syn_packet() {
  if (_syn_ack_received.load()) {
    return false;
  }
  Packet syn_pkt(_pid, PacketType::SYN, 0);
  _socket.send_buf(syn_pkt.serialize());

  return true;
}

void StubbornLink::stop() {
//  std::cerr << "Stopping stubborn link..." << std::endl;
  _stop.store(true);
  // Notify that SYN has been received
  {
    std::lock_guard<std::mutex> lock(_syn_mutex);
    _syn_received.store(true);
  }
  _syn_received_cv.notify_all();
}

//void StubbornLink::resend_syn_packet() {
//  const int interval_ms = 500;
//
//  while (true) {
//    std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
//
//    if (_stop_thread.load()) {
//      std::cerr << "Stopping SYN resend thread..." << std::endl;
//      break;
//    }
//
//    Packet syn_pkt(_pid, PacketType::SYN, 0);
//    _socket.send_buf(syn_pkt.serialize());
//  }
//}

//void StubbornLink::resend_unacked_packets() {
//  const int initial_interval_ms = 100;
//  const int max_interval_ms = 5000;
//
//  int current_interval_ms = initial_interval_ms;
//  uint32_t initial_window_size = 1000;
//  uint32_t window_size = initial_window_size;
//
//  // Wait for a SYN packet to be received from the receiver.
//  {
//    std::unique_lock<std::mutex> syn_lock(_syn_mutex);
//    _syn_received_cv.wait(syn_lock, [this] { return _syn_received.load(); });
//  }
//  std::cerr << "[DEBUG] Unblocked! Sending packets..." << std::endl;
//
//  while (true) {
//    std::unique_lock<std::mutex> interval_lock(_interval_mutex);
//    // Wait for either the timeout or for a signal to stop the thread
//    _resend_cv.wait_for(interval_lock, std::chrono::milliseconds(current_interval_ms), [this] {
//        return _stop_thread.load();
//    });
//
//    if (_stop_thread.load()) {
//      break;
//    }
//
//    std::unique_lock<std::mutex> lock(_unacked_mutex);
//    if (unacked_packets.empty()) {
//      continue;
//    }
//
////    bool success = false;
//    uint32_t packets_sent = 0;
//    for (const auto& pkt : unacked_packets) {
//      if (packets_sent >= window_size) {
//        window_size = initial_window_size;
//        current_interval_ms = std::min(backoff_interval(current_interval_ms), max_interval_ms);
//        break;
//      }
//
//      auto pkt_serialized = pkt.serialize();
//
//      ssize_t nsent = _socket.send_buf(pkt_serialized);
//      if (nsent == -1) {
//        if (errno == ECONNREFUSED) {
//          current_interval_ms = std::min(backoff_interval(current_interval_ms), max_interval_ms);
//        } else if (errno == EWOULDBLOCK) {
//          // Buffer is full. Reduce window size.
//          current_interval_ms = std::min(backoff_interval(current_interval_ms), max_interval_ms);
//          window_size /= 2;
//          break;
//        } else {
//          perror("send failed");
//        }
//      } else {
//        packets_sent++;
//      }
//    }
//    lock.unlock();
//
////    if (_ack_received.load()) {
////      _ack_received.store(false);
////      current_interval_ms = initial_interval_ms;
////      window_size *= 2;
////    } else {
////      current_interval_ms = std::min(backoff_interval(current_interval_ms), max_interval_ms);
////      window_size /= 2;
////    }
//  }
//}

int StubbornLink::backoff_interval(int timeout) {
  std::uniform_int_distribution<int> distribution(timeout, 2 * timeout);
  return distribution(_random_engine);
}