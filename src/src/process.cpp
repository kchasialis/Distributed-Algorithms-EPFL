#include <cstring>
#include <utility>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fstream>
#include <cassert>
#include "process.hpp"
#include "perfect_link.hpp"

Process::Process(uint64_t pid, in_addr_t addr, uint16_t port,
                 const std::vector<Parser::Host>& hosts, const Config &cfg,
                 const std::string& outfname)
        : _pid(pid), _addr(addr), _port(port), _hosts(hosts), _outfile(outfname),
          _n_messages(cfg.num_messages() * (_hosts.size() - 1)) {

  if (cfg.receiver_proc() != _pid) {
    _pl = new PerfectLink(pid, _addr, _port, true, _hosts, cfg.receiver_proc(),
                          _event_loop, [](const Packet& pkt) {
        Process::sender_deliver_callback(pkt);
    });
  } else {
    _pl = new PerfectLink(pid, _addr, _port, false, _hosts, cfg.receiver_proc(),
                          _event_loop, [this](const Packet& pkt) {
        this->receiver_deliver_callback(pkt);
    });
  }

//  uint32_t n_threads = std::thread::hardware_concurrency();
//  if (n_threads == 0) {
//    n_threads = 2;
//  }
  _thread_pool = new ThreadPool(2);

  for (uint32_t i = 0; i < event_loop_workers; i++) {
    _thread_pool->enqueue([this] {
      this->_event_loop.run();
    });
  }
}

Process::~Process() {
  std::cerr << "[DEBUG] Stopping thread pool..." << std::endl;
  _thread_pool->stop();
  std::cerr << "[DEBUG] Closing the file..." << std::endl;
  _outfile.close();
  std::cerr << "[DEBUG] Deleting the PerfectLink instance..." << std::endl;
  delete _pl;
  delete _thread_pool;
}

uint64_t Process::pid() const {
  return _pid;
}

void Process::run(const Config& cfg) {
  if (cfg.receiver_proc() != _pid) {
    run_sender(cfg);
  } else {
    run_receiver(cfg);
  }
}

void Process::stop() {
  std::cerr << "[DEBUG] Stopping process: " << _pid << std::endl;

  std::cerr << "[DEBUG] Stopping event loop..." << std::endl;
  _pl->stop();
  _event_loop.stop();
//  _outfile.flush();
}

EventLoop& Process::event_loop() {
  return _event_loop;
}

void Process::run_sender(const Config& cfg) {
  struct sockaddr_in recv_addr{};
  bool found = false;
  for (const auto& host : _hosts) {
    if (host.id == cfg.receiver_proc()) {
      recv_addr.sin_family = AF_INET;
      recv_addr.sin_port = host.port;
      recv_addr.sin_addr.s_addr = host.ip;
      found = true;
      break;
    }
  }
  assert(found);

//  uint32_t current_seq_id = 1;
//  for (uint32_t i = 0; i < cfg.num_messages(); i += 8) {
//    uint32_t packet_size = std::min(8U, cfg.num_messages() - i);
//    std::vector<uint8_t> data(packet_size * sizeof(uint32_t));
//    for (uint32_t j = 0; j < packet_size; j++) {
//      std::memcpy(data.data() + j * sizeof(uint32_t), &current_seq_id, sizeof(uint32_t));
//      _outfile << "b " << current_seq_id << "\n";
//      current_seq_id++;
//    }
//
//    Packet p(_pid, PacketType::DATA, (i / 8) + 1, data);
//    _pl->send(p, cfg.receiver_proc());
//  }
  _pl->send(cfg.num_messages(), cfg.receiver_proc(), _outfile);

//  _event_loop.run();

  std::cerr << "BYE BRO!" << std::endl;
}

void Process::run_receiver(const Config& cfg) {
  assert (cfg.receiver_proc() == _pid);

  _pl->send_syn_packets();
  std::cerr << "[DEBUG] Finished sending SYN packets!" << std::endl;

  _event_loop.run();

  std::cerr << "BYE BRO!" << std::endl;
}

void Process::sender_deliver_callback(const Packet& pkt) {
  (void) pkt;
}

void Process::receiver_deliver_callback(const Packet& pkt) {
  for (size_t i = 0; i < pkt.data().size(); i += sizeof(uint32_t)) {
    uint32_t seq_id;
    std::memcpy(&seq_id, pkt.data().data() + i, sizeof(uint32_t));
    std::cerr << "Delivered message with seq_id: " << seq_id << std::endl;
    _outfile << "d " << pkt.pid() << " " << seq_id << "\n";
    assert(_n_messages > 0);
    --_n_messages;
    if (_n_messages == 0) {
      std::cerr << "[DEBUG] Delivered all messages!" << std::endl;
    }
  }
//  _outfile << "d " << pkt.pid() << " " << pkt.seq_id() << "\n";
}
