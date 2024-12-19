#include <iostream>
#include <vector>
#include <cstring>
#include "lattice_messages.hpp"

LatticeMessage::LatticeMessage(LatticeMessageType type, std::vector<uint8_t>&& data) :
  _type(type), _data(std::move(data)) {}

void LatticeMessage::serialize(std::vector<uint8_t> &buffer) {
  buffer.clear();

  auto type = static_cast<uint8_t>(_type);
  buffer.push_back(type);

  buffer.insert(buffer.end(), _data.begin(), _data.end());
}

void LatticeMessage::deserialize(const std::vector<uint8_t> &buffer) {
  if (buffer.empty()) {
    std::cerr << "[LatticeMessage] Received an empty buffer for deserialization!" << std::endl;
    return;
  }

  _type = static_cast<LatticeMessageType>(buffer[0]);

  _data.assign(buffer.begin() + 1, buffer.end());
}

void LatticeMessage::set_data(std::vector<uint8_t>&& data) {
  _data = std::move(data);
}

const std::vector<uint8_t>& LatticeMessage::get_data() const {
  return _data;
}

LatticeMessageType LatticeMessage::type() const {
  return _type;
}

ProposalMessage::ProposalMessage(const std::vector<uint32_t> &proposal, uint32_t active_proposal_number) :
  _proposal(proposal), _active_proposal_number(active_proposal_number) {}

void ProposalMessage::serialize(std::vector<uint8_t> &buffer) {
  buffer.clear();
  buffer.reserve(sizeof(uint32_t) + _proposal.size() * sizeof(uint32_t) + sizeof(uint32_t));

  auto num_proposals = static_cast<uint32_t>(_proposal.size());
  buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&num_proposals),
                reinterpret_cast<uint8_t*>(&num_proposals) + sizeof(num_proposals));

  for (uint32_t p : _proposal) {
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&p),
                  reinterpret_cast<uint8_t*>(&p) + sizeof(p));
  }

  buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&_active_proposal_number),
                reinterpret_cast<uint8_t*>(&_active_proposal_number) + sizeof(_active_proposal_number));
}

void ProposalMessage::deserialize(const std::vector<uint8_t> &buffer) {
  _proposal.clear();

  if (buffer.size() < sizeof(uint32_t)) {
    std::cerr << "[ProposalMessage] Received a buffer too small for deserialization!" << std::endl;
    return;
  }

  size_t offset = 0;

  uint32_t num_proposals;
  std::memcpy(&num_proposals, buffer.data() + offset, sizeof(num_proposals));
  offset += sizeof(num_proposals);

  for (uint32_t i = 0; i < num_proposals; ++i) {
    if (offset + sizeof(uint32_t) > buffer.size()) {
      std::cerr << "[ProposalMessage] Received a buffer too small for deserialization of proposals!" << std::endl;
      return;
    }
    uint32_t p;
    std::memcpy(&p, buffer.data() + offset, sizeof(p));
    offset += sizeof(p);
    _proposal.push_back(p);
  }

  if (offset + sizeof(uint32_t) > buffer.size()) {
    std::cerr << "[ProposalMessage] Received a buffer too small for deserialization of active proposal number!" << std::endl;
    return;
  }
  std::memcpy(&_active_proposal_number, buffer.data() + offset, sizeof(_active_proposal_number));
}

const std::vector<uint32_t>& ProposalMessage::proposal() const {
  return _proposal;
}

uint32_t ProposalMessage::active_proposal_number() const {
  return _active_proposal_number;
}