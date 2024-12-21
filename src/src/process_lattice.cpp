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
//  auto num_proposals = _cfg.num_proposals();

//  uint32_t current_seq_id = 1;
//  for (uint32_t i = 0; i < num_proposals; i += 8) {
//    uint32_t packet_size = std::min(BATCH_MSG_SIZE, num_proposals - i);
//    ProposalMessage proposal_msg;
//    for (uint32_t j = 0; j < packet_size; j++) {
//      Proposal proposal;
//      proposal.proposed_value = _cfg.proposals(i + j);
//      proposal.active_proposal_number = current_seq_id;
//      proposal_msg.add_proposal(std::move(proposal));
//      current_seq_id++;
//    }
//    std::vector<uint8_t> data;
//    data.reserve(packet_size * sizeof(Proposal));
//    proposal_msg.serialize(data);
//
//    LatticeMessage lattice_msg(LatticeMessageType::PROPOSAL, std::move(data));
//    std::vector<uint8_t> lattice_data;
//    lattice_msg.serialize(lattice_data);
//    Packet pkt(_pid, PacketType::DATA, std::move(lattice_data));
//    packets.push_back(std::move(pkt));
//  }

  _round.active = true;
  _round.ack_count = 0;
  _round.nack_count = 0;

  ProposalMessage proposal_msg;
  Proposal proposal;
  proposal.proposed_value = _cfg.proposals(0);
  proposal.active_proposal_number = ++_round.active_proposal_number;
  proposal_msg.add_proposal(std::move(proposal));

  propose(proposal_msg);

  _read_event_loop.run();
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

void ProcessLattice::create_proposal_packet(Proposal&& proposal, Packet& packet) const {
  std::vector<uint8_t> data;
  data.reserve(sizeof(Proposal));

  ProposalMessage proposal_msg;
  proposal_msg.add_proposal(std::move(proposal));
  proposal_msg.serialize(data);

  LatticeMessage lattice_msg(LatticeMessageType::PROPOSAL, std::move(data));
  std::vector<uint8_t> lattice_data;
  lattice_msg.serialize(lattice_data);

  packet = Packet(_pid, PacketType::DATA, std::move(lattice_data));
}

void ProcessLattice::create_accept_packet(Accept&& accept, Packet& packet) const {
  std::vector<uint8_t> data;
  data.reserve(sizeof(Accept));

  AcceptMessage accept_msg;
  accept_msg.add_accept(std::move(accept));
  accept_msg.serialize(data);

  LatticeMessage lattice_msg(LatticeMessageType::ACCEPT, std::move(data));
  std::vector<uint8_t> lattice_data;
  lattice_msg.serialize(lattice_data);

  packet = Packet(_pid, PacketType::DATA, std::move(lattice_data));
}

void ProcessLattice::beb_deliver(Packet &&pkt) {
  LatticeMessage lattice_msg;
  lattice_msg.deserialize(pkt.data());

  switch (lattice_msg.type()) {
    case LatticeMessageType::PROPOSAL:
    {
      ProposalMessage proposal_msg;
      proposal_msg.deserialize(lattice_msg.data());
      handle_proposal_msg(proposal_msg, pkt.pid());
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

void ProcessLattice::propose(ProposalMessage &proposal_msg) {
  auto proposal = proposal_msg.proposals().front();

  std::vector<Packet> packets;
  Packet pkt;
  create_proposal_packet(std::move(proposal), pkt);
  packets.push_back(std::move(pkt));

  beb_broadcast(packets);
}

void ProcessLattice::decide() {
  std::cerr << "Decided value:" << std::endl;
  for (const auto& value : _round.proposed_value) {
    std::cerr << value << " ";
  }
  std::cerr << std::endl;

  {
    std::lock_guard<std::mutex> lock(_outfile_mutex);
    _outfile << "Decided value: ";
    for (const auto& value : _round.proposed_value) {
      _outfile << value << " ";
    }
  }
  _round.active = false;
}

void ProcessLattice::handle_proposal_msg(const ProposalMessage &proposal_msg, uint64_t sender_pid) {
  auto proposal = proposal_msg.proposals().front();

  std::cerr << "Proposal message received from " << sender_pid << std::endl;
  std::cerr << "Values: " << std::endl;
  for (const auto& value : proposal.proposed_value) {
    std::cerr << value << " ";
  }
  std::cerr << std::endl;

  std::remove_copy_if(
          proposal.proposed_value.begin(),
          proposal.proposed_value.end(),
          std::back_inserter(_round.proposed_value),
          [this](const uint32_t& item) {
              return std::find(_round.proposed_value.begin(),
                               _round.proposed_value.end(), item)
                     != _round.proposed_value.end();
          }
  );

  Accept accept;
  accept.proposal_number = proposal.active_proposal_number;
  if (_round.proposed_value.size() == proposal.proposed_value.size()) {
    std::cerr << "Accepting proposal" << std::endl;
    accept.nack = false;
    accept.accepted_value = std::move(proposal.proposed_value);
  } else {
    std::cerr << "Rejecting proposal" << std::endl;
    accept.nack = true;
    accept.accepted_value = _round.accepted_value;
  }

  Packet pkt;
  create_accept_packet(std::move(accept), pkt);
  if (sender_pid != _pid) {
    _pl->send(pkt, sender_pid);
  } else {
    beb_deliver(std::move(pkt));
  }
}

void ProcessLattice::handle_accept_msg(const AcceptMessage &accept_msg, uint64_t sender_pid) {
  std::cerr << "Accept message received from " << sender_pid << std::endl;
  for (const auto& accept : accept_msg.accepts()) {
    if (accept.nack) {
      handle_nack_msg(accept);
    } else {
      handle_ack_msg(accept);
    }
  }
}

void ProcessLattice::handle_ack_msg(const Accept &accept) {
  std::cerr << "Ack message received" << std::endl;
  std::cerr << "Proposal number: " << accept.proposal_number << std::endl;
  std::cerr << "Round active proposal number: " << _round.active_proposal_number << std::endl;
  if (accept.proposal_number == _round.active_proposal_number) {
    _round.ack_count++;
    check_ack_nack();
  }
}

void ProcessLattice::handle_nack_msg(const Accept &accept) {
  if (accept.proposal_number == _round.active_proposal_number) {
    std::remove_copy_if(
            accept.accepted_value.begin(),
            accept.accepted_value.end(),
            std::back_inserter(_round.proposed_value),
            [this](const uint32_t& item) {
                return std::find(_round.proposed_value.begin(),
                                 _round.proposed_value.end(), item)
                       != _round.proposed_value.end();
            }
    );
    _round.nack_count++;
    check_ack_nack();
  }
}

void ProcessLattice::check_ack_nack() {
  uint32_t majority = static_cast<uint32_t>(_hosts.size()) / 2 + 1;
  std::cerr << "Ack count: " << _round.ack_count << std::endl;
  std::cerr << "Nack count: " << _round.nack_count << std::endl;
  std::cerr << "Majority: " << majority << std::endl;
  std::cerr << "Active: " << _round.active << std::endl;
  if (_round.ack_count >= majority && _round.active) {
    std::cerr << "Deciding..." << std::endl;
    decide();
  } else if (_round.nack_count > 0 && (_round.ack_count + _round.nack_count >= majority) && _round.active) {
    _round.active_proposal_number++;
    _round.ack_count = 0;
    _round.nack_count = 0;

    std::vector<Packet> packets;
    Proposal proposal{_round.proposed_value, _round.active_proposal_number};
    Packet pkt;
    create_proposal_packet(std::move(proposal), pkt);

    std::cerr << "_round.proposed_value values: " << std::endl;
    for (const auto& value : _round.proposed_value) {
      std::cerr << value << " ";
    }
    std::cerr << std::endl;
    packets.push_back(std::move(pkt));
    beb_broadcast(packets);
  }
}
