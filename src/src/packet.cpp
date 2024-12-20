#include <iostream>
#include <cassert>
#include "packet.hpp"

std::atomic<uint32_t> Packet::_global_seq_id{1};

Packet::Packet(uint64_t pid, PacketType type)
        : _pid(pid), _type(type), _seq_id(_global_seq_id++) {}

Packet::Packet(uint64_t pid, PacketType type, const std::vector<uint8_t>& data)
        : _pid(pid), _type(type), _seq_id(_global_seq_id++), _data(data) {}

Packet::Packet(uint64_t pid, PacketType type, std::vector<uint8_t>&& data)
        : _pid(pid), _type(type), _seq_id(_global_seq_id++), _data(std::move(data)) {}

Packet::Packet(uint64_t pid, PacketType type, uint32_t seq_id)
        : _pid(pid), _type(type), _seq_id(seq_id) {}

//Packet::Packet(uint64_t pid, PacketType type, uint32_t seq_id, const std::vector<uint8_t>& data)
//        : _pid(pid), _type(type), _seq_id(seq_id), _data(data) {}
//
//Packet::Packet(uint64_t pid, PacketType type, uint32_t seq_id, std::vector<uint8_t>&& data)
//        : _pid(pid), _type(type), _seq_id(seq_id), _data(std::move(data)) {}

uint64_t Packet::pid() const {
  return _pid;
}

PacketType Packet::type() const {
  return _type;
}

uint32_t Packet::seq_id() const {
  return _seq_id;
}

const std::vector<uint8_t>& Packet::data() const {
  return _data;
}

void Packet::serialize(std::vector<uint8_t>& buffer) const {
  buffer.clear();
  buffer.reserve(HEADER_SIZE + _data.size());

  auto bytes = reinterpret_cast<const uint8_t*>(&_pid);
  buffer.insert(buffer.end(), bytes, bytes + sizeof(_pid));

  bytes = reinterpret_cast<const uint8_t*>(&_type);
  buffer.insert(buffer.end(), bytes, bytes + sizeof(_type));

  bytes = reinterpret_cast<const uint8_t*>(&_seq_id);
  buffer.insert(buffer.end(), bytes, bytes + sizeof(_seq_id));

  auto data_size = static_cast<uint32_t>(_data.size());
  bytes = reinterpret_cast<const uint8_t*>(&data_size);
  buffer.insert(buffer.end(), bytes, bytes + sizeof(data_size));
  buffer.insert(buffer.end(), _data.begin(), _data.end());
}


void Packet::deserialize(const std::vector<uint8_t> &buffer) {
  assert(buffer.size() >= HEADER_SIZE);
  size_t offset = 0;

  std::memcpy(&_pid, buffer.data() + offset, sizeof(_pid));
  offset += sizeof(_pid);

  std::memcpy(&_type, buffer.data() + offset, sizeof(_type));
  offset += sizeof(_type);

  std::memcpy(&_seq_id, buffer.data() + offset, sizeof(_seq_id));
  offset += sizeof(_seq_id);

  uint32_t data_size;
  std::memcpy(&data_size, buffer.data() + offset, sizeof(data_size));
  offset += sizeof(data_size);

  _data.resize(data_size);
  std::memcpy(_data.data(), buffer.data() + offset, data_size);
}

bool Packet::operator==(const Packet& rhs) const {
  return _seq_id == rhs._seq_id;
}