#include <cstring>
#include <utility>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fstream>
#include <cassert>
#include "process.hpp"
#include "perfect_link.hpp"

Process::Process(uint64_t id, in_addr_t addr, uint16_t port, bool sender, std::string outfile,
                 const std::vector<Parser::Host>& hosts) :
        _pid(getpid()), _id(id),
        _link(std::make_unique<PerfectLink>(addr, port, hosts)), _sender(sender),
        _outfile(std::move(outfile)) {

  for (const auto& host : hosts) {
    struct sockaddr_in host_addr{};
    host_addr.sin_family = AF_INET;
    host_addr.sin_port = host.port;
    host_addr.sin_addr.s_addr = host.ip;
    pid_addr_map[host.id] = host_addr;
  }
}

bool Process::operator==(const Process& other) const {
  auto this_addr = this->_link->addr();
  auto other_addr = other._link->addr();

  if (this_addr.sin_addr.s_addr != other_addr.sin_addr.s_addr || this_addr.sin_port != other_addr.sin_port) {
    return false;
  }

  return this->_pid == other._pid && this->_sender == other._sender;
}

int Process::pid() const {
  return _pid;
}

uint64_t Process::id() const {
  return _id;
}

PerfectLink& Process::link() const {
  return *_link;
}

bool Process::sender() const {
  return _sender;
}

void Process::run_sender(const Config& cfg) {
  assert(cfg.receiver_proc() != _id);
  assert(pid_addr_map.find(cfg.receiver_proc()) != pid_addr_map.end());
  for (uint32_t i = 0; i < cfg.num_messages(); i++) {
    Message m{i + 1};
    _link->send(m, pid_addr_map[cfg.receiver_proc()]);
  }
}

void Process::run_receiver(const Config& cfg) {
  while (true) {
    _link->deliver();
  }
}

void Process::run(const Config& cfg) {
  if (_sender) {
    run_sender(cfg);
  } else {
    run_receiver(cfg);
  }
}

bool Process::write_output() {
  std::ofstream ofs(_outfile, std::ios::out);

  if (!ofs) {
    std::string errmsg = "Failed to open file " + _outfile + ". Error: " + std::strerror(errno);
    std::cerr << errmsg << std::endl;
    return false;
  }
  if (_sender) {
    auto sent = _link->sent();

    for (const auto& msg : sent) {
      ofs << "s " << msg.seq_id() << '\n';
    }
  } else {
    auto delivered = _link->delivered();

    for (const auto& dmsg: delivered) {
      ofs << "d " << dmsg.pid << " " << dmsg.msg.seq_id() << '\n';
    }
  }

  ofs.close();

  std::cerr << "Output written to " << _outfile << " by process with id: " << _id << std::endl;

  return true;
}
