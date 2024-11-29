#pragma once

#include <cstdint>

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
