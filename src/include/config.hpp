#pragma once

#include <cstdint>

class Config {
private:
    uint32_t _num_messages;
    uint32_t _receiver_proc;

public:
    Config(uint32_t num_messages, uint32_t receiver_proc);
    uint32_t num_messages() const;
    uint32_t receiver_proc() const;
};
