#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <utility>
#include <cassert>
#include "stubborn_link.hpp"

StubbornLink::StubbornLink(uint64_t pid, in_addr_t addr, uint16_t port,
                           in_addr_t paddr, uint16_t pport,
                           bool sender, EventLoop& event_loop, DeliverCallback deliver_cb) :
                           _socket(addr, port), _sender(sender),
                           _deliver_cb(std::move(deliver_cb)),
                           _pid(pid), _stop(false) {

  struct sockaddr_in peer_addr{};
  peer_addr.sin_family = AF_INET;
  peer_addr.sin_port = pport;
  peer_addr.sin_addr.s_addr = paddr;
  _socket.conn(peer_addr);

  _read_event_handler = new ReadEventHandler(&_socket,
                                             [this](const Packet& pkt) { this->process_packet(pkt); });
  _read_event_data.fd = _socket.infd();
  _read_event_data.handler_obj = _read_event_handler;

  event_loop.add(EPOLLIN, &_read_event_data);
}

void StubbornLink::process_packet(const Packet& pkt) {
  switch (pkt.packet_type()) {
    case PacketType::SYN:
    {
      assert(_sender);
      // Notify that SYN has been received
      {
        std::lock_guard<std::mutex> lock(_syn_mutex);
        _syn_received.store(true);
      }
      _syn_received_cv.notify_all();
      // Send a SYN_ACK.
      assert(pkt.seq_id() == 0);
      Packet ack_pkt(_pid, PacketType::ACK, pkt.seq_id());
      _socket.send_buf(ack_pkt.serialize());
      break;
    }
    case PacketType::ACK:
    {
      if (pkt.seq_id() == 0) {
        _syn_ack_received.store(true);
      } else {
        std::lock_guard<std::mutex> lock(_unacked_mutex);
        unacked_packets.erase(pkt);
      }
      break;
    }
    case PacketType::DATA:
    {
      // It is a data packet.

      // Deliver the data packet.
      _deliver_cb(pkt);

      // Send an ACK.
      Packet ack_pkt(pkt.pid(), PacketType::ACK, pkt.seq_id());
      _socket.send_buf(ack_pkt.serialize());
      break;
    }
    default:
      std::cerr << "Unknown packet type!" << std::endl;
      break;
  }
}

void StubbornLink::store_and_output_messages(uint32_t n_messages,
                                             std::ofstream &outfile,
                                             std::mutex &outfile_mutex) {
  // Send 8 messages at a single packet.
  std::vector<std::string> output_buffers;
  {
    std::lock_guard<std::mutex> lock(_unacked_mutex);
    uint32_t current_seq_id = 1;
    for (uint32_t i = 0; i < n_messages; i += 8) {
      uint32_t packet_size = std::min(8U, n_messages - i);
      std::vector<uint8_t> data(packet_size * sizeof(uint32_t));
      for (uint32_t j = 0; j < packet_size; j++) {
        std::memcpy(data.data() + j * sizeof(uint32_t), &current_seq_id,
                    sizeof(uint32_t));
        output_buffers.push_back("b " + std::to_string(current_seq_id) + "\n");
        current_seq_id++;
      }

      Packet p(_pid, PacketType::DATA, (i / 8) + 1, data);
      unacked_packets.insert(p);
    }
  }

  {
    std::lock_guard<std::mutex> lock(outfile_mutex);
    for (const auto& buffer : output_buffers) {
      outfile << buffer;
    }
    outfile.flush();
  }
}

// Sliding window approach.
void StubbornLink::send_unacked_messages() {
  const int initial_interval_ms = 50;
  const int max_interval_ms = 1000;
  int timeout_interval_ms = initial_interval_ms;
  const uint32_t sliding_window_size = 300;  // Sliding window size

  // Wait for the receiver to start (SYN received).
  {
    std::unique_lock<std::mutex> syn_lock(_syn_mutex);
    _syn_received_cv.wait(syn_lock, [this] { return _syn_received.load(); });
  }

  // Main retransmission loop
  while (!_stop.load()) {
    std::vector<Packet> packets_to_send;

    {
      // Lock and populate packets_to_send with the sliding window size
      std::unique_lock<std::mutex> lock(_unacked_mutex);
      if (unacked_packets.empty()) {
        _stop.store(true);
        std::cerr << "No more unacknowledged packets. Exiting..." << std::endl;
        continue;  // Exit if there are no unacknowledged packets
      }

      auto it = unacked_packets.begin();
      for (size_t i = 0; i < sliding_window_size && it != unacked_packets.end(); ++i, ++it) {
        packets_to_send.push_back(*it);
      }
    }

    // Send packets in the current sliding window
    for (const auto& pkt : packets_to_send) {
      auto pkt_serialized = pkt.serialize();

      ssize_t nsent = _socket.send_buf(pkt_serialized);
      if (nsent == -1) {
        if (errno == ECONNREFUSED || errno == EWOULDBLOCK) {
          // If an error occurs, wait for the timeout before retrying
          timeout_interval_ms = std::min(backoff_interval(timeout_interval_ms), max_interval_ms);
//          std::cerr << "Current interval ms increased to " << timeout_interval_ms << std::endl;
        } else {
          perror("send failed");
          exit(EXIT_FAILURE);
        }
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(timeout_interval_ms));
  }

  std::cerr << "Exiting send_unacked_messages..." << std::endl;
}

void StubbornLink::send(uint32_t n_messages, std::ofstream &outfile, std::mutex &outfile_mutex) {
  store_and_output_messages(n_messages, outfile, outfile_mutex);

  send_unacked_messages();
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
  _stop.store(true);
  // Notify that SYN has been received to unblock sender.
  {
    std::lock_guard<std::mutex> lock(_syn_mutex);
    _syn_received.store(true);
  }
  _syn_received_cv.notify_all();
}

int StubbornLink::backoff_interval(int timeout) {
  std::uniform_int_distribution<int> distribution(timeout, 2 * timeout);
  return distribution(_random_engine);
}