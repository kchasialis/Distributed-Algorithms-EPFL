#pragma once

#include <vector>
#include <cstdint>

enum class LatticeMessageType : uint8_t {
    PROPOSAL = 0,
    ACCEPT = 1,
    DECIDE = 2
};

class LatticeMessage {
public:
  LatticeMessage() = default;
  LatticeMessage(LatticeMessageType type, std::vector<uint8_t>&& data);
  void serialize(std::vector<uint8_t> &buffer);
  void deserialize(const std::vector<uint8_t> &buffer);
  void set_data(std::vector<uint8_t>&& data);
  const std::vector<uint8_t>& get_data() const;
  LatticeMessageType type() const;

private:
  LatticeMessageType _type;
  std::vector<uint8_t> _data;
};

class ProposalMessage {
public:
  ProposalMessage() = default;
  ProposalMessage(const std::vector<uint32_t> &proposal, uint32_t active_proposal_number);
  void serialize(std::vector<uint8_t> &buffer);
  void deserialize(const std::vector<uint8_t> &buffer);
  const std::vector<uint32_t>& proposal() const;
  uint32_t active_proposal_number() const;
private:
  std::vector<uint32_t> _proposal{};
  uint32_t _active_proposal_number = 0;
};

class AcceptMessage {
public:
    void serialize();
    void deserialize();

private:
    // FILLME.
};

struct DecideMessage {
public:
    void serialize();
    void deserialize();
private:
    // FILLME.
};
