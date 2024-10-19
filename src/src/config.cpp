#include "config.hpp"

Config::Config(uint32_t num_messages, uint32_t sender_proc) : _num_messages(num_messages), _sender_proc(sender_proc) {}

uint32_t Config::num_messages() const {
  return _num_messages;
}

uint32_t Config::sender_proc() const {
  return _sender_proc;
}
