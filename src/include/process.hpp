#pragma once

#include <memory>
#include <unordered_map>
#include "perfect_link.hpp"
#include "message.hpp"
#include "config.hpp"

class Process {
private:
  int _pid;
  uint64_t _id;
  std::unique_ptr<PerfectLink> _link;
  bool _sender;
  std::string _outfile;
  std::unordered_map<uint64_t, struct sockaddr_in> pid_addr_map;

  void run_sender(const Config& cfg);
  void run_receiver(const Config& cfg);

public:
  Process(uint64_t id, in_addr_t addr, uint16_t port, bool sender,
          std::string outfile, const std::vector<Parser::Host>& hosts);
  Process(const Process&) = delete;
  Process& operator=(const Process&) = delete;
  Process(Process&&) = default;
  Process& operator=(Process&&) = default;
  ~Process() = default;
  bool operator==(const Process& other) const;
  int pid() const;
  uint64_t id() const;
  bool sender() const;
  PerfectLink& link() const;
  void run(const Config& cfg);
  bool write_output();
};

