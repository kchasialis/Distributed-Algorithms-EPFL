#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <utility>
#include "stubborn_link.hpp"

StubbornLink::StubbornLink(in_addr_t addr, uint16_t port, in_addr_t paddr, uint16_t pport,
                           bool sender, EventLoop& event_loop, DeliverCallback deliver_cb) :
                           _socket(addr, port), _sender(sender), _event_loop(event_loop),
                           _deliver_cb(std::move(deliver_cb)), _stop_thread(false) {

//  _local_addr.sin_family = AF_INET;
//  _local_addr.sin_port = port;
//  _local_addr.sin_addr.s_addr = addr;
//
//  _peer_addr.sin_family = AF_INET;
//  _peer_addr.sin_port = pport;
//  _peer_addr.sin_addr.s_addr = paddr;
  struct sockaddr_in peer_addr{};
  peer_addr.sin_family = AF_INET;
  peer_addr.sin_port = pport;
  peer_addr.sin_addr.s_addr = paddr;
  _socket.conn(peer_addr);

  event_loop.add(_socket.infd(), EPOLLIN, [this](uint32_t events) {
      read_event_handler(events);
  });

  if (_sender) {
    _resend_thread = std::thread(&StubbornLink::resend_unacked_messages, this);
  }
}

StubbornLink::~StubbornLink() {
//  std::cerr << "[DEBUG] StubbornLink::~StubbornLink: Destructing StubbornLink." << std::endl;
//  if (_sender) {
//    _stop_thread = true;
//    if (_resend_thread.joinable()) {
//      _resend_thread.join();
//    }
//  }
  if (_sender) {
    _stop_thread.store(true);

    // Notify the resend thread to wake up early and stop
    _resend_cv.notify_all();

    if (_resend_thread.joinable()) {
      _resend_thread.join();
    }
  }
}

void StubbornLink::read_event_handler(uint32_t events) {
  if (events & EPOLLIN) {
//    std::cerr << "[DEBUG] StubbornLink::read_event_handler: Data is available to read. " <<
//    inet_ntoa(_local_addr.sin_addr) << " " << ntohs(_local_addr.sin_port) << std::endl;

    // Data is available to read.
    std::vector<uint8_t> buffer(RECV_BUF_SIZE, 0);
    while (true) {
//      ssize_t nrecv = _socket.receive(buffer);
      ssize_t nrecv = _socket.recv_buf(buffer);
//      ssize_t nrecv = recv(_in_fd, buffer.data(), buffer.size(), 0);
      if (nrecv == -1) {
        if (errno == EWOULDBLOCK) {
          // No more data to read.
          break;
        }
        std::string err_msg = "read() failed. Error message: ";
        err_msg += strerror(errno);
        perror(err_msg.c_str());
        exit(EXIT_FAILURE);
      }
      if (nrecv == 0) {
        continue;
      }
      // Process the received data.
      Packet pkt;
//      std::cerr << "[DEBUG] Read " << nrecv << " bytes from socket." << std::endl;
      buffer.resize(static_cast<size_t>(nrecv));
      pkt.deserialize(buffer);
      process_packet(pkt);
    }
    _event_loop.rearm(_socket.infd(), EPOLLIN);
  }
}

void StubbornLink::resend_unacked_messages() {
  const int initial_interval_ms = 1000;
  const int max_interval_ms = 16000;

  int current_interval_ms = initial_interval_ms;

//  while (!_stop_thread.load()) {
//    std::this_thread::sleep_for(std::chrono::milliseconds(current_interval_ms));
  while (true) {
    std::unique_lock<std::mutex> interval_lock(_interval_mutex);
    // Wait for either the timeout or for a signal to stop the thread
    _resend_cv.wait_for(interval_lock, std::chrono::milliseconds(current_interval_ms), [this] {
        return _stop_thread.load();
    });

    if (_stop_thread.load()) {
      break;
    }

    std::unique_lock<std::mutex> lock(_unacked_mutex);

    bool success = false;
//    uint32_t packets_sent = 0;
//    std::cerr << "[DEBUG] Resending packets... thread_id: " << gettid() << std::endl;
    for (const auto& pkt : unacked_packets) {
//      if (packets_sent >= _window_size) {
//        break;
//      }

//      std::cerr << "[DEBUG] Resending packet with seq_id: " << seq_id << std::endl;
      auto pkt_serialized = pkt.serialize();

//      ssize_t nsent = _socket.send1(pkt_serialized);
      ssize_t nsent = _socket.send_buf(pkt_serialized);
      if (nsent == -1) {
        if (errno == ECONNREFUSED) {
//          std::cerr << "[DEBUG] Connection refused. Increasing time interval." << std::endl;
          current_interval_ms = std::min(current_interval_ms * 2, max_interval_ms);
        } else if (errno == EWOULDBLOCK) {
//          std::cerr << "[DEBUG] send() would block, retrying for packet seq_id: " << seq_id << std::endl;
        } else {
          perror("send failed");
        }
      } else {
//        std::cerr << "[DEBUG] Successfully resent packet with seq_id: " << seq_id << std::endl;
        success = true;
//          packets_sent++;
      }
    }

    lock.unlock();

    if (success) {
      current_interval_ms = initial_interval_ms;
    }

//    if (packets_sent > 0) {
//      current_interval_ms = initial_interval_ms;
//    }
  }
}

void StubbornLink::process_packet(const Packet& pkt) {
//  std::cerr << "[DEBUG] Processing packet with seq_id: " << pkt.seq_id() << std::endl;
  if (pkt.packet_type() == PacketType::ACK) {
//    std::cerr << "[DEBUG] Received ACK for seq_id: " << pkt.seq_id() << " from " <<
//              inet_ntoa(_peer_addr.sin_addr) << " " << ntohs(_peer_addr.sin_port) << std::endl;
    std::lock_guard<std::mutex> lock(_unacked_mutex);
    unacked_packets.erase(pkt);
//    std::cerr << "[DEBUG] Remaining unacked packets: " << unacked_packets.size() << std::endl;
  } else {
    // It is a data packet.
    // Send an ACK.
//    std::cerr << "[DEBUG] Sending ACK for seq_id : " << pkt.seq_id() << " to peer " <<
//              inet_ntoa(_peer_addr.sin_addr) << " " << ntohs(_peer_addr.sin_port) << std::endl;
    Packet ack_pkt(pkt.pid(), PacketType::ACK, pkt.seq_id());
    _socket.send_buf(ack_pkt.serialize());
//    _socket.send1(pkt_serialized);

    // Deliver the data packet.
    _deliver_cb(pkt);
  }
}

void StubbornLink::send(const Packet& pkt) {
  std::lock_guard<std::mutex> lock(_unacked_mutex);
  unacked_packets.insert(pkt);
//  ssize_t nsent = _socket.send1(pkt_serialized);
  ssize_t nsent = _socket.send_buf(pkt.serialize());
  if (nsent == -1) {
    if (errno == EWOULDBLOCK) {
//      std::cerr << "[DEBUG] " << inet_ntoa(_local_addr.sin_addr) << " " << _local_addr.sin_port << "Sent returned EWOULDBLOCK" << std::endl;
    }
    else if (errno == ECONNREFUSED) {
//      std::cerr << "[DEBUG] " << inet_ntoa(_local_addr.sin_addr) << " " << _local_addr.sin_port << "Sent returned ECONNREFUSED" << std::endl;
    }
  }
}

//void StubbornLink::send(const Packet& p) {
//  {
//    std::lock_guard<std::mutex> lock(_unacked_mutex);
//    unacked_packets[p.seq_id()] = p;
//  }
//  _send_cv.notify_one();  // Wake up the resend thread if it’s waiting
//}

//void StubbornLink::receive_ack(uint32_t seq_id) {
//  std::lock_guard<std::mutex> lock(unacked_mutex);
//  unacked_packets.erase(seq_id);  // Remove from unacked packets on ACK
//}

//void StubbornLink::write_event_handler(uint32_t events) {
//  if (events & EPOLLOUT) {
//    std::lock_guard<std::mutex> queue_lock(queue_mutex);
//
//    while (!resend_queue.empty()) {
//      Packet pkt = resend_queue.front();
//      auto pkt_serialized = pkt.serialize();
//      ssize_t nsent = _socket.send1(pkt_serialized);
//      if (nsent == -1) {
//        if (errno == EWOULDBLOCK) {
//          std::cerr << "[DEBUG] " << inet_ntoa(_local_addr.sin_addr) << " " << _local_addr.sin_port << "Sent returned EWOULDBLOCK" << std::endl;
//          break;
//        }
//        else if (errno == ECONNREFUSED) {
//          std::cerr << "[DEBUG] " << inet_ntoa(_local_addr.sin_addr) << " " << _local_addr.sin_port << "Sent returned ECONNREFUSED" << std::endl;
//          break;
//        }
//        else {
//          std::string err_msg = "send() failed. Error message: ";
//          err_msg += strerror(errno);
//          perror(err_msg.c_str());
//          exit(EXIT_FAILURE);
//        }
//      }
//      resend_queue.pop();
//      std::cerr << "[DEBUG] Resending packet with seq_id: " << pkt.seq_id() << std::endl;
//    }
//
//    _event_loop.rearm(_out_fd, EPOLLOUT);
//  }
//}

//void StubbornLink::write_event_handler(uint32_t events) {
//  if (events & EPOLLOUT) {
////    std::cerr << "[DEBUG] Received event for fd: " << _socket.sockfd() << std::endl;
////    std::cerr << "[DEBUG] StubbornLink::write_event_handler: Writing data to socket." << std::endl;
//    //  Data can be written.
//    //  Send all unacked packets.
//    std::lock_guard<std::mutex> lock(unacked_mutex);
////    std::cerr << "[DEBUG] Sending unacked packets from " << inet_ntoa(_local_addr.sin_addr) << " " << ntohs(_local_addr.sin_port) << std::endl;
//    for (const auto& [seq_id, pkt] : unacked_packets) {
//      auto pkt_serialized = pkt.serialize();
//      ssize_t nsent = _socket.send1(pkt_serialized);
//      if (nsent == -1) {
//        if (errno == EWOULDBLOCK) {
//          std::cerr << "[DEBUG] " << inet_ntoa(_local_addr.sin_addr) << " " << _local_addr.sin_port << "Sent returned EWOULDBLOCK" << std::endl;
//          break;
//        }
//        else if (errno == ECONNREFUSED) {
//          std::cerr << "[DEBUG] " << inet_ntoa(_local_addr.sin_addr) << " " << _local_addr.sin_port << "Sent returned ECONNREFUSED" << std::endl;
//          break;
//        }
//        else {
//          std::string err_msg = "send() failed. Error message: ";
//          err_msg += strerror(errno);
//          perror(err_msg.c_str());
//          exit(EXIT_FAILURE);
//        }
//      }
//    }
//
//    _event_loop.rearm(_out_fd, EPOLLOUT);
//  }
//}

//void StubbornLink::enable_epollout() {
//  _event_loop.modify(_out_fd, EPOLLOUT);
//}
//
//void StubbornLink::disable_epollout() {
//  _event_loop.modify(_out_fd, 0);
//}

//void StubbornLink::send(const Packet& p) {
//  std::lock_guard<std::mutex> lock(unacked_mutex);
//
//  // Check if this is the first unacknowledged packet being added
//  bool was_empty = unacked_packets.empty();
//  unacked_packets[p.seq_id()] = p;
//  auto pkt_serialized = p.serialize();
//  _socket.send1(pkt_serialized);
//
//  // If it was empty, we need to enable EPOLLOUT to handle sending
//  if (was_empty) {
//    enable_epollout();
//  }
//}

//bool StubbornLink::all_acked() {
//  std::lock_guard<std::mutex> lock(unacked_mutex);
//  return unacked_packets.empty();
//}