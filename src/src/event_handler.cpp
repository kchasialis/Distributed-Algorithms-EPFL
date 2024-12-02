#include <cstring>
#include <iostream>
#include "event_handler.hpp"

ReadEventHandler::ReadEventHandler(UDPSocket *socket, ReadCallback process_pkt_callback) :
                                   _socket(socket),
                                   _process_pkt_callback(std::move(process_pkt_callback)) {}

void ReadEventHandler::handle_read_event(uint32_t events) {
  if (events & EPOLLIN) {
    // Data is available to read.
    std::vector<uint8_t> buffer(RECV_BUF_SIZE, 0);
    while (true) {
      buffer.resize(RECV_BUF_SIZE);
      ssize_t nrecv = _socket->recv_buf(buffer);
      if (nrecv == -1) {
        if (errno == EWOULDBLOCK || errno == ECONNREFUSED) {
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
      _process_pkt_callback(pkt);
    }
  }
}

WriteEventHandler::WriteEventHandler(WriteCallback send_pkts_callback) :
                                     _send_pkts_callback(std::move(send_pkts_callback)) {}

void WriteEventHandler::handle_write_event(uint32_t events) {
  if (events & EPOLLOUT) {
    _send_pkts_callback();
  }
}