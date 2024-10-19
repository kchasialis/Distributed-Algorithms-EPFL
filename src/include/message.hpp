#pragma once

#include <cstdlib>
#include <functional>

class Message {
private:
  int _seq_id;

public:
  explicit Message(int seq_id);
  int seq_id() const;
  const void *data() const;
  void *data();
  std::size_t size() const;
  bool operator==(const Message& other) const;
};

struct MessageHash {
  std::size_t operator()(const Message& msg) const {
      return std::hash<int>()(msg.seq_id());
  }
};

struct MessageEqual {
  bool operator()(const Message& lhs, const Message& rhs) const {
      return lhs == rhs;
  }
};
