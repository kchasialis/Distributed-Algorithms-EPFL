#pragma once

#include <cstdint>

class Config {
private:
    uint32_t _num_messages;
    uint32_t _sender_proc;

public:
    Config(uint32_t num_messages, uint32_t sender_proc);
    uint32_t num_messages() const;
    uint32_t sender_proc() const;
};
