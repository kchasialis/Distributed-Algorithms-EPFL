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
  for (uint32_t i = 0; i < num_proposals; i++) {
    ProposalMessage proposal_msg(_cfg.proposals(i), i);
    std::vector<uint8_t> data;
    proposal_msg.serialize(data);
    LatticeMessage lattice_msg(LatticeMessageType::PROPOSAL, std::move(data));
    std::vector<uint8_t> lattice_data;
    lattice_msg.serialize(lattice_data);
    Packet pkt(_pid, PacketType::DATA, i + 1, std::move(lattice_data));
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

void ProcessLattice::beb_deliver(Packet &&pkt) {
  LatticeMessage lattice_msg;
  lattice_msg.deserialize(pkt.data());

  switch (lattice_msg.type()) {
    case LatticeMessageType::PROPOSAL:
    {
      std::cerr << "Proposal message received" << std::endl;
      ProposalMessage proposal_msg;
      std::cerr << "Active proposal number: " << proposal_msg.active_proposal_number() << std::endl;
      proposal_msg.deserialize(lattice_msg.get_data());
      auto proposal = proposal_msg.proposal();
      std::cerr << "Proposal message has " << proposal.size() << " values" << std::endl;
      for (auto p : proposal) {
        std::cerr << p << " ";
      }
      std::cerr << std::endl;
      break;
    }
    case LatticeMessageType::ACCEPT:
      std::cerr << "Accept message received" << std::endl;
      break;
    case LatticeMessageType::DECIDE:
      std::cerr << "Decide message received" << std::endl;
      break;
    default:
      std::cerr << "Unknown lattice message type!" << std::endl;
  }
}
