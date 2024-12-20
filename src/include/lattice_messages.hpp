#pragma once

#include <vector>
#include <cstdint>
#include <array>

enum class LatticeMessageType : uint8_t {
    PROPOSAL = 0,
    ACCEPT = 1,
    DECIDE = 2
};

// Batch 8 messages at a time.
constexpr uint32_t BATCH_MSG_SIZE = 8;

class LatticeMessage {
public:
  LatticeMessage() = default;
  LatticeMessage(LatticeMessageType type, std::vector<uint8_t>&& data);
  void serialize(std::vector<uint8_t> &buffer);
  void deserialize(const std::vector<uint8_t> &buffer);
  const std::vector<uint8_t>& data() const;
  LatticeMessageType type() const;

private:
  LatticeMessageType _type;
  std::vector<uint8_t> _data;
};

struct Proposal {
  std::vector<uint32_t> proposed_value{};
  uint32_t active_proposal_number = 0;
};

class ProposalMessage {
public:
  ProposalMessage();
  void add_proposal(Proposal &&proposal);
  const std::vector<Proposal>& proposals() const;
  void serialize(std::vector<uint8_t> &buffer);
  void deserialize(const std::vector<uint8_t> &buffer);
private:
  std::vector<Proposal> _proposals;
};

struct Accept {
  bool nack;
  uint32_t proposal_number;
  std::vector<uint32_t> accepted_value;
};

class AcceptMessage {
public:
  AcceptMessage();
  void add_accept(Accept &&accept);
  const std::vector<Accept>& accepts() const;
  void serialize(std::vector<uint8_t> &buffer);
  void deserialize(const std::vector<uint8_t> &buffer);

private:
  std::vector<Accept> _accepts;
};

struct Decide {
  // NOTE(kostas): FILLME.
};

class DecideMessage {
public:
  DecideMessage() = default;
  void add_decision(Decide &&decision);
  void serialize(std::vector<uint8_t> &buffer);
  void deserialize(const std::vector<uint8_t> &buffer);
private:
  std::vector<Decide> _decisions;
};
