#include "process_lattice.hpp"

#include <utility>

ProcessLattice::ProcessLattice(uint64_t pid, in_addr_t addr, uint16_t port,
                               const std::vector<Parser::Host> &hosts, LatticeConfig cfg,
                               const std::string& outfname) :
                               _pid(pid), _addr(addr), _port(port), _cfg(std::move(cfg)),
                               _hosts(hosts), _outfile(outfname, std::ios::out | std::ios::trunc) {

  _pl = new PerfectLink(pid, addr, port, hosts, _read_event_loop, _write_event_loop,
                        [this](Packet &&pkt) {
                            this->beb_deliver(std::move(pkt));
                        });

  _thread_pool = new ThreadPool(8);

//  for (uint32_t i = 0; i < read_event_loop_workers; i++) {
//    _thread_pool->enqueue([this] {
//        this->_read_event_loop.run();
//    });
//  }
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
//  std::vector<ProposalMessage> proposal_messages;
  for (uint32_t i = 0; i < num_proposals; i += BATCH_MSG_SIZE) {
    uint32_t packet_size = std::min(BATCH_MSG_SIZE, num_proposals - i);
    ProposalMessage proposal_msg;
//    std::vector<Round> rounds;
    for (uint32_t j = 0; j < packet_size; j++) {
      Proposal proposal{_cfg.proposals(i + j), current_round, 0};
      proposal_msg.add_proposal(std::move(proposal));
//      rounds.emplace_back(current_round, _cfg.proposals(i + j));
      current_round++;
    }
//    set_rounds(std::move(rounds));

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

void ProcessLattice::beb_broadcast(std::vector<Packet> &packets) {
  for (const auto& host : _hosts) {
    if (host.id != _pid) {
      _pl->send(packets, host.id);
    }
  }

  for (auto& pkt : packets) {
    beb_deliver(std::move(pkt));
  }
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
  std::unique_lock<std::mutex> lock(_round_mutex);
  for (auto& proposal : proposal_msg.proposals()) {
    if (proposal.round < _rounds.size()) {
      _rounds[proposal.round].proposed_value = proposal.proposed_value;
      _rounds[proposal.round].active = true;
    } else {
      Round round(proposal.proposed_value);
      round.active = true;
      _rounds.push_back(std::move(round));
    }
    std::cerr << "Proposing for round: " << proposal.round << std::endl;
//    std::cerr << "Proposal number: " << proposal.active_proposal_number << std::endl;
    std::cerr << "Proposed value: ";
    for (const auto& value : proposal.proposed_value) {
      std::cerr << value << " ";
    }
    std::cerr << std::endl;
  }
  lock.unlock();

  Packet pkt = create_proposal_packet(proposal_msg);
  beb_broadcast(std::move(pkt));
}

void ProcessLattice::decide(Round &round) {
  std::cerr << "Decided value:" << std::endl;
  for (const auto& value : round.proposed_value) {
    std::cerr << value << " ";
  }
  std::cerr << std::endl;

  {
    std::lock_guard<std::mutex> lock(_outfile_mutex);
    _outfile << "Decided value: ";
    for (const auto& value : round.proposed_value) {
      _outfile << value << " ";
    }
  }
  round.active = false;
}

void ProcessLattice::handle_proposal_msg(ProposalMessage &&proposal_msg, uint64_t sender_pid) {
//  std::cerr << "Proposal message received from " << sender_pid << " with " << proposal_msg.proposals().size() << " proposals " << std::endl;

  AcceptMessage accept_msg;
  for (auto& proposal : proposal_msg.proposals()) {
    Accept accept;
    handle_proposal(std::move(proposal), accept);
    accept_msg.add_accept(std::move(accept));
  }

//  std::cerr << "Sending accept message to " << sender_pid << " with " << accept_msg.accepts().size() << " accepts" << std::endl;

  Packet pkt = create_accept_packet(accept_msg);
  if (sender_pid != _pid) {
    _pl->send(pkt, sender_pid);
  } else {
    beb_deliver(std::move(pkt));
  }
}

void ProcessLattice::handle_proposal(Proposal &&proposal, Accept &accept) {
//  auto &round = get_round(proposal.round);

  std::unique_lock<std::mutex> lock(_round_mutex);

  while (proposal.round >= _rounds.size()) {
    _rounds.emplace_back();
  }

  auto& round = _rounds[proposal.round];

//  std::cerr << "Handling proposal for round: " << proposal.round << std::endl;
//  std::cerr << "proposal.active_proposal_number: " << proposal.active_proposal_number << std::endl;
//  std::cerr << "Round accepted_value: ";
//  for (const auto& value : round.accepted_value) {
//    std::cerr << value << " ";
//  }
//  std::cerr << std::endl;
//
//  std::cerr << "Proposed value: ";
//  for (const auto& value : proposal.proposed_value) {
//    std::cerr << value << " ";
//  }
//  std::cerr << std::endl;

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

//  std::cerr << "After combining accepted_value: ";
//  for (const auto& value : round.accepted_value) {
//    std::cerr << value << " ";
//  }
//  std::cerr << std::endl;

  accept.proposal_number = proposal.active_proposal_number;
  accept.round = proposal.round;
  if (round.accepted_value.size() == proposal.proposed_value.size()) {
//    std::cerr << "Sending ACK prop number: " << proposal.active_proposal_number << " for round: " << proposal.round << std::endl;
    round.accepted_value = std::move(proposal.proposed_value);
    accept.nack = false;
  } else {
//    std::cerr << "Sending NACK prop number: " << proposal.active_proposal_number << " for round: " << proposal.round << std::endl;
    accept.nack = true;
    accept.accepted_value = round.accepted_value;
  }

//  std::cerr << std::endl;

  lock.unlock();
}

void ProcessLattice::handle_accept_msg(const AcceptMessage &accept_msg, uint64_t sender_pid) {
//  std::cerr << "Accept message received from " << sender_pid << " with " << accept_msg.accepts().size() << " accepts " << std::endl;
  for (const auto& accept : accept_msg.accepts()) {
    if (accept.nack) {
      handle_nack_msg(accept);
    } else {
      handle_ack_msg(accept);
    }
  }
}

void ProcessLattice::handle_ack_msg(const Accept &accept) {

//  auto &round = get_round(accept.round);

//  std::cerr << "Ack message for round: " << accept.round << std::endl;
//  std::cerr << "Proposal number: " << accept.proposal_number << std::endl;
//  std::cerr << "Accepted value: ";
//  for (const auto& value : accept.accepted_value) {
//    std::cerr << value << " ";
//  }
//  std::cerr << std::endl;

  std::unique_lock<std::mutex> lock(_round_mutex);

  while (accept.round >= _rounds.size()) {
    _rounds.emplace_back();
  }

  auto& round = _rounds[accept.round];

//  std::cerr << "Active: " << round.active << std::endl;

  if (!round.active) {
    lock.unlock();
    return;
  }

//  std::cerr << "Active proposal number: " << round.active_proposal_number << std::endl;

  if (accept.proposal_number == round.active_proposal_number) {
    round.ack_count++;
    check_ack_nack(round, accept.round, std::move(lock));
  }
}

void ProcessLattice::handle_nack_msg(const Accept &accept) {
//  auto &round = get_round(accept.round);

//  std::cerr << "NAck message for round: " << accept.round << std::endl;
//  std::cerr << "Proposal number: " << accept.proposal_number << std::endl;

  std::unique_lock<std::mutex> lock(_round_mutex);

  while (accept.round >= _rounds.size()) {
    _rounds.emplace_back();
  }

  auto& round = _rounds[accept.round];

//  std::cerr << "Round active: " << round.active << std::endl;

  if (!round.active) {
    lock.unlock();
    return;
  }

  if (accept.proposal_number == round.active_proposal_number) {
//    std::cerr << "Before combining:" << std::endl;
//    std::cerr << "accept.accepted_value: ";
//    for (const auto& value : accept.accepted_value) {
//      std::cerr << value << " ";
//    }
//    std::cerr << std::endl;
//
//    std::cerr << "round.proposed value: ";
//    for (const auto& value : round.proposed_value) {
//      std::cerr << value << " ";
//    }
//    std::cerr << std::endl;

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

//    std::cerr << "After combining:" << std::endl;
//    std::cerr << "round.proposed value: ";
//    for (const auto& value : round.proposed_value) {
//      std::cerr << value << " ";
//    }
//    std::cerr << std::endl;

    check_ack_nack(round, accept.round, std::move(lock));
  }
}

void ProcessLattice::check_ack_nack(Round &round, uint32_t roundi, std::unique_lock<std::mutex>&& lock) {
//  std::cerr << "Ack count: " << round.ack_count << std::endl;
//  std::cerr << "Nack count: " << round.nack_count << std::endl;
  uint32_t f = static_cast<uint32_t>(_hosts.size()) / 2 + 1;
  if (round.ack_count >= f) {
    std::cerr << "Deciding for round: " << roundi << std::endl;
//    std::cerr << std::endl;
    decide(round);
    lock.unlock();
  } else if (round.nack_count > 0 && (round.ack_count + round.nack_count >= f)) {
    round.active_proposal_number++;
    round.ack_count = 0;
    round.nack_count = 0;

//    std::cerr << "Proposing again for round: " << roundi << std::endl;
//    std::cerr << "Proposed value: ";
//    for (const auto& value : round.proposed_value) {
//      std::cerr << value << " ";
//    }
//    std::cerr << std::endl;

    ProposalMessage proposal_message;
    proposal_message.add_proposal(Proposal{round.proposed_value, roundi, round.active_proposal_number});
    lock.unlock();

//    std::cerr << "Sending proposal message with " << proposal_message.proposals().size() << " proposals" << std::endl;

    Packet pkt = create_proposal_packet(proposal_message);
    beb_broadcast(std::move(pkt));
  } else {
//    std::cerr << std::endl;
    lock.unlock();
  }
}

//Round& ProcessLattice::get_round(uint32_t roundi) {
//  std::lock_guard<std::mutex> lock(_round_mutex);
//  if (roundi >= _rounds.size()) {
//    _rounds.resize(roundi + 1);
//    _rounds[roundi] = Round(roundi);
//  }
//  return _rounds[roundi];
//}

//void ProcessLattice::set_rounds(std::vector<Round> &&rounds) {
//  std::lock_guard<std::mutex> lock(_round_mutex);
//  for (auto & round : rounds) {
//    _rounds.push_back(std::make_shared<Round>(std::move(round)));
////    if (round.round_number >= _rounds.size()) {
////      _rounds.resize(round.round_number + 1);
////      _rounds[round.round_number] = std::make_shared<Round>(round);
////    } else {
////      _rounds[round.round_number] = std::make_shared<Round>(std::move(round));
////    }
//  }
//}
