#pragma once

#include <map>
#include "parser.hpp"
#include "perfect_link.hpp"
#include "packet.hpp"
#include "config.hpp"
#include "event_loop.hpp"
#include "thread_pool.hpp"
#include "lattice_messages.hpp"

struct Round {
  uint32_t round_number{0};
  bool active{false};
  uint32_t ack_count{0};
  uint32_t nack_count{0};
  uint32_t active_proposal_number{0};
  std::vector<uint32_t> proposed_value;
  std::vector<uint32_t> accepted_value;
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
  EventLoop _read_event_loop;
  EventLoop _write_event_loop;
  ThreadPool *_thread_pool;
  PerfectLink *_pl;
  LatticeConfig _cfg;
  std::vector<Parser::Host> _hosts;
//  std::vector<std::mutex> _rounds_mutexes;
  std::mutex _round_mutex;
  std::vector<Round> _rounds;
  std::mutex _decisions_mutex;
  std::unordered_map<uint32_t, std::vector<uint32_t>> _decisions;
  uint32_t _next_round_to_output;
  std::chrono::steady_clock::time_point _start_time;
  std::mutex _outfile_mutex;
  std::ofstream _outfile;

  void beb_broadcast(Packet &&packet);
  void beb_deliver(Packet &&pkt);
  Packet create_proposal_packet(ProposalMessage &proposal_msg) const;
  Packet create_accept_packet(const AcceptMessage &accept_msg) const;
  void propose(ProposalMessage &proposal_msg);
  void decide(Round &round, std::unique_lock<std::mutex>&& lock);
  void handle_proposal_msg(ProposalMessage &&proposal_msg, uint64_t sender_pid);
  void handle_proposal(Proposal &&proposal, Accept &accept);
  void handle_accept_msg(const AcceptMessage &accept_msg, uint64_t sender_pid);
  void handle_ack_msg(const Accept &accept);
  void handle_nack_msg(const Accept &accept);
  void check_ack_nack(Round &round, std::unique_lock<std::mutex>&& lock);
};
