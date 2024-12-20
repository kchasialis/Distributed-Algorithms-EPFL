#include <iostream>
#include <vector>
#include <cstring>
#include "lattice_messages.hpp"

LatticeMessage::LatticeMessage(LatticeMessageType type, std::vector<uint8_t>&& data) :
  _type(type), _data(std::move(data)) {}

void LatticeMessage::serialize(std::vector<uint8_t> &buffer) {
  buffer.clear();

  buffer.push_back(static_cast<uint8_t>(_type));

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

const std::vector<uint8_t>& LatticeMessage::data() const {
  return _data;
}

LatticeMessageType LatticeMessage::type() const {
  return _type;
}

ProposalMessage::ProposalMessage() {
  _proposals.reserve(BATCH_MSG_SIZE);
}

void ProposalMessage::add_proposal(Proposal &&proposal) {
  _proposals.push_back(std::move(proposal));
}

const std::vector<Proposal>& ProposalMessage::proposals() const {
  return _proposals;
}

void ProposalMessage::serialize(std::vector<uint8_t> &buffer) {
  buffer.clear();

  auto n_proposals = static_cast<uint32_t>(_proposals.size());
  buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&n_proposals),
                reinterpret_cast<uint8_t*>(&n_proposals) + sizeof(n_proposals));

  for (auto &proposal : _proposals) {
    auto n_values = static_cast<uint32_t>(proposal.proposed_value.size());
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&n_values),
                  reinterpret_cast<uint8_t*>(&n_values) + sizeof(n_values));

    for (uint32_t value : proposal.proposed_value) {
      buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&value),
                    reinterpret_cast<uint8_t*>(&value) + sizeof(value));
    }

    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&proposal.active_proposal_number),
                  reinterpret_cast<uint8_t*>(&proposal.active_proposal_number) + sizeof(proposal.active_proposal_number));
  }
}

void ProposalMessage::deserialize(const std::vector<uint8_t> &buffer) {
  _proposals.clear();

  size_t offset = 0;

  if (buffer.size() < sizeof(uint32_t)) {
    std::cerr << "[ProposalMessage] Buffer too small to read number of proposals" << std::endl;
    return;
  }

  uint32_t n_proposals;
  std::memcpy(&n_proposals, buffer.data() + offset, sizeof(n_proposals));
  offset += sizeof(n_proposals);

  if (n_proposals > BATCH_MSG_SIZE) {
    std::cerr << "[ProposalMessage] Number of proposals exceeds maximum allowed (8)" << std::endl;
    return;
  }

  for (uint32_t i = 0; i < n_proposals; ++i) {
    if (offset + sizeof(uint32_t) > buffer.size()) {
      std::cerr << "[ProposalMessage] Buffer too small to read number of values in proposal" << std::endl;
      return;
    }
    uint32_t n_values;
    std::memcpy(&n_values, buffer.data() + offset, sizeof(n_values));
    offset += sizeof(n_values);

    if (offset + n_values * sizeof(uint32_t) > buffer.size()) {
      std::cerr << "[ProposalMessage] Buffer too small to read values in proposal" << std::endl;
      return;
    }
    std::vector<uint32_t> values(n_values);
    for (uint32_t j = 0; j < n_values; ++j) {
      std::memcpy(&values[j], buffer.data() + offset, sizeof(uint32_t));
      offset += sizeof(uint32_t);
    }

    if (offset + sizeof(uint32_t) > buffer.size()) {
      std::cerr << "[ProposalMessage] Buffer too small to read active proposal number" << std::endl;
      return;
    }
    uint32_t active_proposal_number;
    std::memcpy(&active_proposal_number, buffer.data() + offset, sizeof(active_proposal_number));
    offset += sizeof(active_proposal_number);

    _proposals.push_back({std::move(values), active_proposal_number});
  }
}

AcceptMessage::AcceptMessage() {
  _accepts.reserve(BATCH_MSG_SIZE);
}

void AcceptMessage::add_accept(Accept &&accept) {
  _accepts.push_back(std::move(accept));
}

const std::vector<Accept>& AcceptMessage::accepts() const {
  return _accepts;
}

void AcceptMessage::serialize(std::vector<uint8_t> &buffer) {
  buffer.clear();

  auto n_accepts = static_cast<uint32_t>(_accepts.size());
  buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&n_accepts),
                reinterpret_cast<uint8_t*>(&n_accepts) + sizeof(n_accepts));

  for (Accept &accept : _accepts) {
    bool nack_flag = static_cast<bool>(accept.nack);
    buffer.push_back(nack_flag);

    // Add the proposal number
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&accept.proposal_number),
                  reinterpret_cast<uint8_t*>(&accept.proposal_number) + sizeof(accept.proposal_number));

    auto n_values = static_cast<uint32_t>(accept.accepted_value.size());
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&n_values),
                  reinterpret_cast<uint8_t*>(&n_values) + sizeof(n_values));

    for (uint32_t value : accept.accepted_value) {
      buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&value),
                    reinterpret_cast<uint8_t*>(&value) + sizeof(value));
    }
  }
}

void AcceptMessage::deserialize(const std::vector<uint8_t> &buffer) {
  _accepts.clear();

  size_t offset = 0;

  if (buffer.size() < sizeof(uint32_t)) {
    std::cerr << "[AcceptMessage] Buffer too small to read number of accepts" << std::endl;
    return;
  }

  uint32_t n_accepts;
  std::memcpy(&n_accepts, buffer.data() + offset, sizeof(n_accepts));
  offset += sizeof(n_accepts);

  for (uint32_t i = 0; i < n_accepts; ++i) {
    if (offset + sizeof(uint8_t) > buffer.size()) {
      std::cerr << "[AcceptMessage] Buffer too small to read nack flag" << std::endl;
      return;
    }
    bool nack;
    std::memcpy(&nack, buffer.data() + offset, sizeof(nack));
    offset += sizeof(nack);

    if (offset + sizeof(uint32_t) > buffer.size()) {
      std::cerr << "[AcceptMessage] Buffer too small to read proposal number" << std::endl;
      return;
    }

    uint32_t proposal_number;
    std::memcpy(&proposal_number, buffer.data() + offset, sizeof(proposal_number));
    offset += sizeof(proposal_number);

    if (offset + sizeof(uint32_t) > buffer.size()) {
      std::cerr << "[AcceptMessage] Buffer too small to read number of values in accepted_value" << std::endl;
      return;
    }

    uint32_t n_values;
    std::memcpy(&n_values, buffer.data() + offset, sizeof(n_values));
    offset += sizeof(n_values);

    if (offset + n_values * sizeof(uint32_t) > buffer.size()) {
      std::cerr << "[AcceptMessage] Buffer too small to read values in accepted_value" << std::endl;
      return;
    }
    std::vector<uint32_t> accepted_value(n_values);
    for (uint32_t j = 0; j < n_values; ++j) {
      std::memcpy(&accepted_value[j], buffer.data() + offset, sizeof(uint32_t));
      offset += sizeof(uint32_t);
    }

    _accepts.push_back({nack, proposal_number, std::move(accepted_value)});
  }
}