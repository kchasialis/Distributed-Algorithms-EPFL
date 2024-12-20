#pragma once

#include <cstdlib>
#include <cstring>
#include <functional>
#include <vector>
#include <atomic>

enum class PacketType {
    DATA,
    ACK,
};

constexpr static size_t HEADER_SIZE = sizeof(uint64_t) + sizeof(PacketType) + sizeof(uint32_t) + sizeof(uint32_t);

class Packet {
private:
    uint64_t _pid;
    PacketType _type;
    uint32_t _seq_id;
    std::vector<uint8_t> _data;

    static std::atomic<uint32_t> _global_seq_id;

public:
    Packet() = default;
    Packet(uint64_t pid, PacketType type);
    Packet(uint64_t pid, PacketType type, const std::vector<uint8_t>& data);
    Packet(uint64_t pid, PacketType type, std::vector<uint8_t>&& data);
    Packet(uint64_t pid, PacketType type, uint32_t seq_id);
//    Packet(uint64_t pid, PacketType type, uint32_t seq_id, const std::vector<uint8_t>& data);
//    Packet(uint64_t pid, PacketType type, uint32_t seq_id, std::vector<uint8_t>&& data);
    uint64_t pid() const;
    PacketType type() const;
    uint32_t seq_id() const;
    const std::vector<uint8_t>& data() const;
    void serialize(std::vector<uint8_t> &buffer) const;
    void deserialize(const std::vector<uint8_t>& buffer);

    bool operator==(const Packet& rhs) const;
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

struct PacketLess {
    bool operator()(const Packet& lhs, const Packet& rhs) const {
      return std::less<uint32_t>()(lhs.seq_id(), rhs.seq_id());
    }
};
