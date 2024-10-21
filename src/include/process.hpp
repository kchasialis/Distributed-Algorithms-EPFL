#pragma once

#include <memory>

#include "perfect_link.hpp"
#include "message.hpp"

class Process {
private:
  int _pid;
  uint64_t _id;
  std::unique_ptr<PerfectLink> _link;
  bool _sender;

public:
  Process(uint64_t id, in_addr_t addr, uint16_t port, bool sender);
  Process(const Process&) = delete;
  Process& operator=(const Process&) = delete;
  Process(Process&&) = default;
  Process& operator=(Process&&) = default;
  ~Process() = default;
  int pid() const;
  uint64_t id() const;
  bool sender() const;
  PerfectLink& link() const;
  bool operator==(const Process& other) const;
};

