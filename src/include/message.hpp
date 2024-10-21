#pragma once

#include <cstdlib>
#include <functional>

class Message {
private:
  uint32_t _seq_id;

public:
  Message() = default;
  explicit Message(uint32_t seq_id);
  uint32_t seq_id() const;
  const void *data() const;
  void *data();
  std::size_t size() const;
  bool operator==(const Message& other) const;
};

struct MessageHash {
  std::size_t operator()(const Message& msg) const {
      return std::hash<uint32_t>()(msg.seq_id());
  }
};

struct MessageEqual {
  bool operator()(const Message& lhs, const Message& rhs) const {
      return lhs == rhs;
  }
};
