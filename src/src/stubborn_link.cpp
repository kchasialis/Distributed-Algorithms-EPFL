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
                           _pid(pid), _stop(false) {

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
      {
        std::lock_guard<std::mutex> lock(_unacked_mutex);
//        bool print_msg = false;
//        if (_unacked_packets.size() == 1) {
//          print_msg = true;
//        }
        _unacked_packets.erase(pkt);
//        if (_unacked_packets.empty() && print_msg) {
//          std::cerr << "NO MORE UNACKED PACKETS FROM: " << ntohs(_pport) << std::endl;
//        }
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

      Packet ack_pkt(_pid, PacketType::ACK, pkt.seq_id());
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

//  // Send 8 messages at a single packet.
//  std::vector<std::string> output_buffers;
//  {
//    std::lock_guard<std::mutex> lock(_unacked_mutex);
//    uint32_t current_seq_id = 1;
//    for (uint32_t i = 0; i < n_messages; i += 8) {
//      uint32_t packet_size = std::min(8U, n_messages - i);
//      std::vector<uint8_t> data(packet_size * sizeof(uint32_t));
//      for (uint32_t j = 0; j < packet_size; j++) {
//        std::memcpy(data.data() + j * sizeof(uint32_t), &current_seq_id,
//                    sizeof(uint32_t));
//        output_buffers.push_back("b " + std::to_string(current_seq_id) + "\n");
//        current_seq_id++;
//      }
//
//      Packet p(_pid, PacketType::DATA, (i / 8) + 1, data);
//      unacked_packets.insert(p);
//    }
//  }
//
//  {
//    std::lock_guard<std::mutex> lock(outfile_mutex);
//    for (const auto& buffer : output_buffers) {
//      outfile << buffer;
//    }
//    outfile.flush();
//  }
//}

// Sliding window approach.
void StubbornLink::send_unacked_packets() {
  const int initial_interval_ms = 500;
  const int max_interval_ms = 1000;
  int timeout_interval_ms = initial_interval_ms;
  const uint32_t sliding_window_size = 300;  // Sliding window size

  // Main retransmission loop
  while (!_stop.load()) {
    std::vector<Packet> packets_to_send;

    {
      // Lock and populate packets_to_send with the sliding window size
      std::unique_lock<std::mutex> lock(_unacked_mutex);
      if (_unacked_packets.empty()) {
        return;  // Exit if there are no unacknowledged packets
      }

      auto it = _unacked_packets.begin();
      for (size_t i = 0; i < sliding_window_size && it != _unacked_packets.end(); ++i, ++it) {
        packets_to_send.push_back(*it);
      }
    }

//    std::cerr << "Resending " << packets_to_send.size() << " to " << _pport << std::endl;

    // Send packets in the current sliding window
    size_t sent = 0;
    for (const auto& pkt : packets_to_send) {

      auto pkt_serialized = pkt.serialize();
      ssize_t nsent = _socket.send_buf(pkt_serialized);
//      ssize_t nsent = _socket.sendto_buf(pkt_serialized, _peer_addr);
      if (nsent == -1) {
        if (errno == EWOULDBLOCK) {
          std::cerr << "OUTPUT BUFFER BLOCKED" << std::endl;
          // Socket buffer full, increase timeout and wait.
          break;
        } else if (errno == ECONNREFUSED) {
          // Receiver is not up and running, try later.
          return;
        } else {
          perror("send failed");
          exit(EXIT_FAILURE);
        }
      } else {
        sent++;
      }
    }

    if (sent == packets_to_send.size()) {
      timeout_interval_ms = initial_interval_ms;
    } else {
      timeout_interval_ms = std::min(backoff_interval(timeout_interval_ms),
                                     max_interval_ms);
    }

//    std::this_thread::sleep_for(std::chrono::milliseconds(timeout_interval_ms));
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
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