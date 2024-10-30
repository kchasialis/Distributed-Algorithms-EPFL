#pragma once

#include <cstdlib>
#include <cstring>
#include <functional>

enum class PacketType {
    DATA,
    ACK,
};

constexpr static size_t HEADER_SIZE = sizeof(int) + sizeof(PacketType) + sizeof(uint32_t) + sizeof(uint32_t);

class Packet {
private:
    uint64_t _pid;
    PacketType _type;
    uint32_t _seq_id;
    std::vector<uint8_t> _data;

public:
    Packet() = default;
    Packet(uint64_t pid, PacketType type, uint32_t seq_id);
    Packet(uint64_t pid, PacketType type, uint32_t seq_id, const std::vector<uint8_t>& data);
    uint64_t pid() const;
    PacketType packet_type() const;
    uint32_t seq_id() const;
    const std::vector<uint8_t>& data() const;
    std::vector<uint8_t> serialize() const;
    void deserialize(const std::vector<uint8_t>& buffer);
};

struct PacketHash {
    std::size_t operator()(const Packet& pkt) const {
      std::hash<uint32_t> hash_seq_id;
      return hash_seq_id(pkt.seq_id());
    }
};

struct PacketEqual {
    bool operator()(const Packet& lhs, const Packet& rhs) const {
      return lhs.seq_id() == rhs.seq_id();
    }
};