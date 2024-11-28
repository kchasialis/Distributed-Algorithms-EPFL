#pragma once

#include <cstdint>
#include <utility>
#include "udp_socket.hpp"
#include "event_loop.hpp"
#include "packet.hpp"

using DeliverCallback = std::function<void(const Packet& pkt)>;

class ReadEventHandler {
public:
    ReadEventHandler(UDPSocket *socket, DeliverCallback process_pkt_callback);
    void handle_read_event(uint32_t events);

private:
    UDPSocket *_socket;
    DeliverCallback _process_pkt_callback;
};
