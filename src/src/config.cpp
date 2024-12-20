#include "config.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

PlConfig::PlConfig(const std::string &config_path) {
  std::ifstream config_file(config_path);
  if (!config_file) {
    std::cerr << "Failed to open config file: " << config_path << std::endl;
    exit(1);
  }

  uint32_t n_messages, receiver_proc;
  if (!(config_file >> n_messages >> receiver_proc)) {
    std::cerr << "Failed to read config values from: " << config_path << std::endl;
    exit(1);
  }

  _num_messages = n_messages;
  _receiver_proc = receiver_proc;
}

uint32_t PlConfig::num_messages() const {
  return _num_messages;
}

uint32_t PlConfig::receiver_proc() const {
  return _receiver_proc;
}

FifoConfig::FifoConfig(const std::string &config_path) {
  std::ifstream config_file(config_path);
  if (!config_file) {
    std::cerr << "Failed to open config file: " << config_path << std::endl;
    exit(1);
  }

  uint32_t n_messages;
  if (!(config_file >> n_messages)) {
    std::cerr << "Failed to read config values from: " << config_path << std::endl;
    exit(1);
  }

  _num_messages = n_messages;
}

uint32_t FifoConfig::num_messages() const {
  return _num_messages;
}

LatticeConfig::LatticeConfig(const std::string &config_path) {
  std::ifstream config_file(config_path);
  if (!config_file) {
    std::cerr << "Failed to open config file: " << config_path << std::endl;
    exit(1);
  }

  uint32_t n_proposals, max_elems_prop;
  if (!(config_file >> n_proposals >> max_elems_prop >> _max_distinct_all_prop)) {
    std::cerr << "Failed to read config values from: " << config_path << std::endl;
    exit(1);
  }

  _proposals.reserve(n_proposals);
  config_file.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // First line already read. Skip it.
  for (uint32_t i = 0; i < n_proposals; ++i) {
    // Each line can have up to max_elems_prop.
    std::vector<uint32_t> proposal_set;
    proposal_set.reserve(max_elems_prop);
    std::string line;
    if (!std::getline(config_file, line)) {
      std::cerr << "Failed to read line " << i + 1 << " from: " << config_path << std::endl;
      exit(1);
    }

    std::istringstream line_stream(line);
    uint32_t elem;
    while (line_stream >> elem) {
      proposal_set.push_back(elem);
    }
    if (proposal_set.size() > max_elems_prop) {
      std::cerr << "Proposal set " << i + 1 << " has more than maximum allowed elements (" << max_elems_prop << ")" << std::endl;
      exit(1);
    }

    _proposals.push_back(std::move(proposal_set));
  }
  if (_proposals.size() != n_proposals) {
    std::cerr << "Number of proposals read does not match the expected number" << std::endl;
    exit(1);
  }
}

uint32_t LatticeConfig::max_distinct_props() const {
  return _max_distinct_all_prop;
}

uint32_t LatticeConfig::num_proposals() const {
  return static_cast<uint32_t>(_proposals.size());
}

const std::vector<uint32_t>& LatticeConfig::proposals(uint32_t idx) const {
  return _proposals[idx];
}
