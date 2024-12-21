#pragma once

#include "parser.hpp"
#include "perfect_link.hpp"
#include "packet.hpp"
#include "config.hpp"
#include "event_loop.hpp"
#include "thread_pool.hpp"
#include "lattice_messages.hpp"

struct Round {
  bool active;
  uint32_t ack_count;
  uint32_t nack_count;
  uint32_t active_proposal_number;
  std::vector<uint32_t> proposed_value;
  std::vector<uint32_t> accepted_value;

  Round() : active(false), ack_count(0), nack_count(0), active_proposal_number(0) {}
};

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
//  std::vector<Round> _rounds;
  Round _round; // for one round only now.
  std::mutex _outfile_mutex;
  std::ofstream _outfile;

  void beb_broadcast(std::vector<Packet>& packets);
  void beb_deliver(Packet &&pkt);
  void create_proposal_packet(Proposal&& proposal, Packet& packet) const;
  void create_accept_packet(Accept&& accept, Packet& packet) const;
  void propose(ProposalMessage &proposal_msg);
  void decide();
  void handle_proposal_msg(const ProposalMessage &proposal_msg, uint64_t sender_pid);
  void handle_accept_msg(const AcceptMessage &accept_msg, uint64_t sender_pid);
  void handle_ack_msg(const Accept &accept);
  void handle_nack_msg(const Accept &accept);
  void check_ack_nack();
};
