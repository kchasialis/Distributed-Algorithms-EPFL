#pragma once

#include <cstdint>
#include <cstddef>

// These have to add up to 8.
constexpr uint32_t read_event_loop_workers = 4;
constexpr uint32_t write_event_loop_workers = 2;
constexpr uint32_t monitor_delivery_workers = 2;
static_assert(read_event_loop_workers + write_event_loop_workers + monitor_delivery_workers == 8);

class PlConfig {
private:
    uint32_t _num_messages;
    uint32_t _receiver_proc;

public:
    PlConfig(uint32_t num_messages, uint32_t receiver_proc);
    uint32_t num_messages() const;
    uint32_t receiver_proc() const;
};

class FifoConfig {
private:
    uint32_t _num_messages;

public:
    FifoConfig(uint32_t num_messages);
    uint32_t num_messages() const;
};
