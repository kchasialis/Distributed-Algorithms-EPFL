#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

constexpr uint32_t read_event_loop_workers = 5;
constexpr uint32_t write_event_loop_workers = 3;
static_assert(read_event_loop_workers + write_event_loop_workers <= 8);

class PlConfig {
private:
    uint32_t _num_messages;
    uint32_t _receiver_proc;

public:
    PlConfig(const std::string &config_path);
    uint32_t num_messages() const;
    uint32_t receiver_proc() const;
};

class FifoConfig {
private:
    uint32_t _num_messages;

public:
    FifoConfig(const std::string &config_path);
    uint32_t num_messages() const;
};

class LatticeConfig {
private:
  uint32_t _max_distinct_all_prop;
  std::vector<std::vector<uint32_t>> _proposals;

public:
  LatticeConfig(const std::string &config_path);
  uint32_t num_proposals() const;
  const std::vector<uint32_t>& proposals(uint32_t idx) const;
};
