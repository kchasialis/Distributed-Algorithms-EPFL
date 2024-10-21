#include "config.hpp"

Config::Config(uint32_t num_messages, uint32_t receiver_proc) : _num_messages(num_messages), _receiver_proc(receiver_proc) {}

uint32_t Config::num_messages() const {
  return _num_messages;
}

uint32_t Config::receiver_proc() const {
  return _receiver_proc;
}
