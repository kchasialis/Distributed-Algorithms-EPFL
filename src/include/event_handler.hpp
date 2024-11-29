#pragma once

#include <cstdint>
#include <utility>
#include "udp_socket.hpp"
#include "event_loop.hpp"
#include "packet.hpp"

using ReadCallback = std::function<void(const Packet& pkt)>;
using WriteCallback = std::function<void()>;

class ReadEventHandler {
public:
    ReadEventHandler(UDPSocket *socket, ReadCallback process_pkt_callback);
    void handle_read_event(uint32_t events);

private:
    UDPSocket *_socket;
    ReadCallback _process_pkt_callback;
};

class WriteEventHandler {
public:
    explicit WriteEventHandler(WriteCallback send_pkts_callback);
    void handle_write_event(uint32_t events);
private:
    WriteCallback _send_pkts_callback;
};