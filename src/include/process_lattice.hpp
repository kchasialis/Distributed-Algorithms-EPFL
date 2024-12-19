#pragma once

#include "parser.hpp"
#include "perfect_link.hpp"
#include "packet.hpp"
#include "config.hpp"
#include "event_loop.hpp"
#include "thread_pool.hpp"
#include "lattice_messages.hpp"

class ProcessLattice {
public:
  ProcessLattice(uint64_t pid, in_addr_t addr, uint16_t port,
              const std::vector<Parser::Host> &hosts, LatticeConfig cfg,
              const std::string& outfname);
  ~ProcessLattice();

  void stop();
  void run();
private:
  uint64_t _pid;
  in_addr_t _addr;
  uint16_t _port;
  EventLoop _read_event_loop;
  EventLoop _write_event_loop;
  ThreadPool *_thread_pool;
  PerfectLink *_pl;
  LatticeConfig _cfg;
  std::vector<Parser::Host> _hosts;
  std::mutex _outfile_mutex;
  std::ofstream _outfile;

  void beb_broadcast(std::vector<Packet>& packets);
  void beb_deliver(Packet &&pkt);
};
