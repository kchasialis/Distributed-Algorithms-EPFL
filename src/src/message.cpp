#include "message.hpp"

Message::Message(int seq_id) : _seq_id(seq_id) {}

int Message::seq_id() const {
  return _seq_id;
}

const void *Message::data() const {
  return static_cast<const void *>(&_seq_id);
}

void *Message::data() {
  return static_cast<void *>(&_seq_id);
}

std::size_t Message::size() const {
  return sizeof(_seq_id);
}

bool Message::operator==(const Message& other) const {
  return _seq_id == other._seq_id;
}
