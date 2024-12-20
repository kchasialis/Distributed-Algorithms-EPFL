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

  uint32_t current_seq_id = 1;
  for (uint32_t i = 0; i < num_proposals; i += 8) {
    uint32_t packet_size = std::min(BATCH_MSG_SIZE, num_proposals - i);
    ProposalMessage proposal_msg;
    for (uint32_t j = 0; j < packet_size; j++) {
      Proposal proposal;
      proposal.proposed_value = _cfg.proposals(i + j);
      proposal.active_proposal_number = current_seq_id;
      proposal_msg.add_proposal(std::move(proposal));
      current_seq_id++;
    }
    std::vector<uint8_t> data;
    data.reserve(packet_size * sizeof(Proposal));
    proposal_msg.serialize(data);

    LatticeMessage lattice_msg(LatticeMessageType::PROPOSAL, std::move(data));
    std::vector<uint8_t> lattice_data;
    lattice_msg.serialize(lattice_data);
    Packet pkt(_pid, PacketType::DATA, std::move(lattice_data));
    packets.push_back(std::move(pkt));
  }

  beb_broadcast(packets);

  _read_event_loop.run();
}

void ProcessLattice::beb_broadcast(std::vector<Packet> &packets) {
  for (const auto& host : _hosts) {
    if (host.id != _pid) {
      _pl->send(packets, host.id);
    }
  }
}

// TODO(kostas): Create a function that creates lattice messages.
void ProcessLattice::beb_deliver(Packet &&pkt) {
  LatticeMessage lattice_msg;
  lattice_msg.deserialize(pkt.data());

  switch (lattice_msg.type()) {
    case LatticeMessageType::PROPOSAL:
    {
      ProposalMessage proposal_msg;
      proposal_msg.deserialize(lattice_msg.data());
//      for (const auto& proposal : proposal_msg.proposals()) {
//        std::cerr << "Active proposal number: " << proposal.active_proposal_number << std::endl;
//        auto value = proposal.proposed_value;
//        std::cerr << "Proposal message has " << value.size() << " values" << std::endl;
//        for (auto v : value) {
//          std::cerr << v << " ";
//        }
//        std::cerr << std::endl;
//      }

      // Send an accept back.
      AcceptMessage accept_msg;
      for (const auto& proposal : proposal_msg.proposals()) {
        Accept accept;
        accept.nack = false;
        accept.proposal_number = proposal.active_proposal_number;
        accept.accepted_value = proposal.proposed_value;
        accept_msg.add_accept(std::move(accept));
      }
      std::vector<uint8_t> accept_msg_serialized;
      accept_msg.serialize(accept_msg_serialized);

      LatticeMessage accept_lattice_msg(LatticeMessageType::ACCEPT, std::move(accept_msg_serialized));
      std::vector<uint8_t> lattice_data;
      accept_lattice_msg.serialize(lattice_data);

      Packet accept_pkt(_pid, PacketType::DATA, lattice_data);
      std::cerr << "Sending accept message with seq_id: " << accept_pkt.seq_id() << std::endl;
      _pl->send(accept_pkt, pkt.pid());
      break;
    }
    case LatticeMessageType::ACCEPT:
    {
      std::cerr << "Accept message received with seq_id: " << pkt.seq_id() << " from " << pkt.pid() << std::endl;
//      AcceptMessage accept_msg;
//      accept_msg.deserialize(lattice_msg.data());
//      for (const auto& accept : accept_msg.accepts()) {
//        std::cerr << "Accept message has proposal number: " << accept.proposal_number << std::endl;
//        std::cerr << "Accept message has " << accept.accepted_value.size() << " values" << std::endl;
//        for (auto v : accept.accepted_value) {
//          std::cerr << v << " ";
//        }
//        std::cerr << std::endl;
//      }
      break;
    }
    case LatticeMessageType::DECIDE:
      std::cerr << "Decide message received" << std::endl;
      break;
    default:
      std::cerr << "Unknown lattice message type!" << std::endl;
  }
}
