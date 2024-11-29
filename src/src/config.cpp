#include "config.hpp"

PlConfig::PlConfig(uint32_t num_messages, uint32_t receiver_proc) : _num_messages(num_messages), _receiver_proc(receiver_proc) {}

uint32_t PlConfig::num_messages() const {
  return _num_messages;
}

uint32_t PlConfig::receiver_proc() const {
  return _receiver_proc;
}

FifoConfig::FifoConfig(uint32_t num_messages) : _num_messages(num_messages) {}

uint32_t FifoConfig::num_messages() const {
  return _num_messages;
}
