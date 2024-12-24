#include "process_lattice.hpp"

#include <utility>

ProcessLattice::ProcessLattice(uint64_t pid, in_addr_t addr, uint16_t port,
                               const std::vector<Parser::Host> &hosts, LatticeConfig cfg,
                               const std::string& outfname) :
                               _pid(pid), _cfg(std::move(cfg)), _hosts(hosts),
                               _rounds(_cfg.num_proposals()), _next_round_to_output(0),
                               _outfile(outfname, std::ios::out | std::ios::trunc) {

  for (uint32_t i = 0; i < _cfg.num_proposals(); i++) {
    _rounds[i].round_number = i;
  }

  _pl = new PerfectLink(pid, addr, port, hosts, _read_event_loop, _write_event_loop,
                        [this](Packet &&pkt) {
                            this->beb_deliver(std::move(pkt));
                        });

  _thread_pool = new ThreadPool(8);

  for (uint32_t i = 0; i < read_event_loop_workers; i++) {
    _thread_pool->enqueue([this] {
        this->_read_event_loop.run();
    });
  }
  for (uint32_t i = 0; i < write_event_loop_workers; i++) {
    _thread_pool->enqueue([this] {
        this->_write_event_loop.run();
    });
  }
}

ProcessLattice::~ProcessLattice() {
  std::cerr << "Goodbye from process " << _pid << std::endl;
  _thread_pool->stop();
  _outfile.close();
  delete _pl;
  delete _thread_pool;
}

void ProcessLattice::stop() {
  {
    std::lock_guard<std::mutex> lock(_outfile_mutex);
    _outfile.flush();
  }
  _pl->stop();
  _write_event_loop.stop();
  _read_event_loop.stop();
}

void ProcessLattice::run() {
  std::vector<Packet> packets;
  auto num_proposals = _cfg.num_proposals();

  uint32_t current_round = 0;
  for (uint32_t i = 0; i < num_proposals; i += BATCH_MSG_SIZE) {
    uint32_t packet_size = std::min(BATCH_MSG_SIZE, num_proposals - i);
    ProposalMessage proposal_msg;
    for (uint32_t j = 0; j < packet_size; j++) {
      Proposal proposal{_cfg.proposals(i + j), current_round, 0};
      proposal_msg.add_proposal(std::move(proposal));
      current_round++;
    }

    propose(proposal_msg);
  }

  _read_event_loop.run();
}

void ProcessLattice::beb_broadcast(Packet &&packet) {
  for (const auto& host : _hosts) {
    if (host.id != _pid) {
      _pl->send(packet, host.id);
    }
  }

  beb_deliver(std::move(packet));
}

void ProcessLattice::beb_deliver(Packet &&pkt) {
  LatticeMessage lattice_msg;
  lattice_msg.deserialize(pkt.data());

  switch (lattice_msg.type()) {
    case LatticeMessageType::PROPOSAL:
    {
      ProposalMessage proposal_msg;
      proposal_msg.deserialize(lattice_msg.data());
      handle_proposal_msg(std::move(proposal_msg), pkt.pid());
      break;
    }
    case LatticeMessageType::ACCEPT:
    {
      AcceptMessage accept_msg;
      accept_msg.deserialize(lattice_msg.data());
      handle_accept_msg(accept_msg, pkt.pid());
      break;
    }
    default:
      std::cerr << "Unknown lattice message type!" << std::endl;
      exit(EXIT_FAILURE);
  }
}

Packet ProcessLattice::create_proposal_packet(ProposalMessage &proposal_msg) const {
  std::vector<uint8_t> data;
  data.reserve(sizeof(Proposal) * proposal_msg.proposals().size());
  proposal_msg.serialize(data);

  LatticeMessage lattice_msg(LatticeMessageType::PROPOSAL, std::move(data));
  std::vector<uint8_t> lattice_data;
  lattice_msg.serialize(lattice_data);

  return Packet(_pid, PacketType::DATA, std::move(lattice_data));
}

Packet ProcessLattice::create_accept_packet(const AcceptMessage &accept_msg) const {
  std::vector<uint8_t> data;
  data.reserve(sizeof(Accept) * accept_msg.accepts().size());
  accept_msg.serialize(data);

  LatticeMessage lattice_msg(LatticeMessageType::ACCEPT, std::move(data));
  std::vector<uint8_t> lattice_data;
  lattice_msg.serialize(lattice_data);

  return Packet(_pid, PacketType::DATA, std::move(lattice_data));
}

void ProcessLattice::propose(ProposalMessage &proposal_msg) {
  {
    std::lock_guard<std::mutex> lock(_round_mutex);
    for (auto& proposal : proposal_msg.proposals()) {
      _rounds[proposal.round].proposed_value = proposal.proposed_value;
      _rounds[proposal.round].active = true;
    }
  }

  beb_broadcast(create_proposal_packet(proposal_msg));
}

void ProcessLattice::decide(Round &round, std::unique_lock<std::mutex>&& lock) {
  std::cerr << "Decided value:" << std::endl;
  for (const auto& value : round.proposed_value) {
    std::cerr << value << " ";
  }
  std::cerr << std::endl;
  round.active = false;
  lock.unlock();

  {
    std::lock_guard<std::mutex> lock_decisions(_decisions_mutex);

    _decisions[round.round_number] = round.proposed_value;

    while (_decisions.find(_next_round_to_output) != _decisions.end()) {
      const auto& decided_values = _decisions[_next_round_to_output];

      {
        std::lock_guard<std::mutex> lock_outfile(_outfile_mutex);
        for (size_t i = 0; i < decided_values.size(); ++i) {
          _outfile << decided_values[i];
          if (i < decided_values.size() - 1) {
            _outfile << " ";
          }
        }
        _outfile << "\n";
      }

      _decisions.erase(_next_round_to_output);

      _next_round_to_output++;
    }
  }
}

void ProcessLattice::handle_proposal_msg(ProposalMessage &&proposal_msg, uint64_t sender_pid) {
  AcceptMessage accept_msg;
  for (auto& proposal : proposal_msg.proposals()) {
    Accept accept;
    handle_proposal(std::move(proposal), accept);
    accept_msg.add_accept(std::move(accept));
  }

  Packet pkt = create_accept_packet(accept_msg);
  if (sender_pid != _pid) {
    _pl->send(pkt, sender_pid);
  } else {
    beb_deliver(std::move(pkt));
  }
}

void ProcessLattice::handle_proposal(Proposal &&proposal, Accept &accept) {
  std::lock_guard<std::mutex> lock(_round_mutex);

  auto& round = _rounds[proposal.round];

  std::remove_copy_if(
          proposal.proposed_value.begin(),
          proposal.proposed_value.end(),
          std::back_inserter(round.accepted_value),
          [&round](const uint32_t& item) {
              return std::find(round.accepted_value.begin(),
                               round.accepted_value.end(), item)
                     != round.accepted_value.end();
          }
  );

  accept.proposal_number = proposal.active_proposal_number;
  accept.round = proposal.round;
  if (round.accepted_value.size() == proposal.proposed_value.size()) {
    round.accepted_value = std::move(proposal.proposed_value);
    accept.nack = false;
  } else {
    accept.nack = true;
    accept.accepted_value = round.accepted_value;
  }
}

void ProcessLattice::handle_accept_msg(const AcceptMessage &accept_msg, uint64_t sender_pid) {
  for (const auto& accept : accept_msg.accepts()) {
    if (accept.nack) {
      handle_nack_msg(accept);
    } else {
      handle_ack_msg(accept);
    }
  }
}

void ProcessLattice::handle_ack_msg(const Accept &accept) {
  std::unique_lock<std::mutex> lock(_round_mutex);

  auto& round = _rounds[accept.round];

  if (!round.active) {
    lock.unlock();
    return;
  }

  if (accept.proposal_number == round.active_proposal_number) {
    round.ack_count++;
    check_ack_nack(round, std::move(lock));
  }
}

void ProcessLattice::handle_nack_msg(const Accept &accept) {
  std::unique_lock<std::mutex> lock(_round_mutex);

  auto& round = _rounds[accept.round];

  if (!round.active) {
    lock.unlock();
    return;
  }

  if (accept.proposal_number == round.active_proposal_number) {
    std::remove_copy_if(
            accept.accepted_value.begin(),
            accept.accepted_value.end(),
            std::back_inserter(round.proposed_value),
            [&round](const uint32_t& item) {
                return std::find(round.proposed_value.begin(),
                                 round.proposed_value.end(), item)
                       != round.proposed_value.end();
            }
    );
    round.nack_count++;

    check_ack_nack(round, std::move(lock));
  }
}

void ProcessLattice::check_ack_nack(Round &round, std::unique_lock<std::mutex>&& lock) {
  uint32_t f = static_cast<uint32_t>(_hosts.size()) / 2 + 1;
  if (round.ack_count >= f) {
    decide(round, std::move(lock));
  } else if (round.nack_count > 0 && (round.ack_count + round.nack_count >= f)) {
    round.active_proposal_number++;
    round.ack_count = 0;
    round.nack_count = 0;

    ProposalMessage proposal_message;
    proposal_message.add_proposal(Proposal{round.proposed_value, round.round_number, round.active_proposal_number});
    lock.unlock();

    beb_broadcast(create_proposal_packet(proposal_message));
  } else {
    lock.unlock();
  }
}

