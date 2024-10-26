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
        : _pid(pid), _addr(addr), _port(port), _hosts(hosts), _outfile(outfname) {

  if (cfg.receiver_proc() != _pid) {
    _pl = new PerfectLink(_addr, _port, true, _hosts, _event_loop, [](const std::vector<uint8_t>& data) {
        Process::sender_deliver_callback(data);
    });
  } else {
    _pl = new PerfectLink(_addr, _port, false, _hosts, _event_loop, [this](const std::vector<uint8_t>& data) {
        this->receiver_deliver_callback(data);
    });
  }

  uint32_t n_threads = std::thread::hardware_concurrency();
  if (n_threads == 0) {
    n_threads = 2;
  }
  _thread_pool = new ThreadPool(n_threads);

  for (uint32_t i = 0; i < event_loop_workers; i++) {
    _thread_pool->enqueue([this] {
      this->_event_loop.run();
    });
  }
}

Process::~Process() {
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
//  std::cerr << "[DEBUG] Stopping process: " << _pid << std::endl;

//  std::cerr << "[DEBUG] Stopping event loop..." << std::endl;
  _event_loop.stop();
//  std::cerr << "[DEBUG] Stopping thread pool..." << std::endl;
  _thread_pool->stop();
//  std::cerr << "[DEBUG] Closing the file..." << std::endl;
  _outfile.close();
//  std::cerr << "[DEBUG] Deleting the PerfectLink instance..." << std::endl;
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

  for (uint32_t i = 1; i <= cfg.num_messages(); i++) {
    std::vector<uint8_t> data;
    data.resize(sizeof(uint32_t));
    std::memcpy(data.data(), &i, sizeof(uint32_t));
    Packet p(_pid, PacketType::DATA, i, data);
    _outfile << "b " << p.seq_id() << "\n";
    _pl->send(p, cfg.receiver_proc());
  }

  _event_loop.run();

  std::cerr << "BYE LOL" << std::endl;
}

void Process::run_receiver(const Config& cfg) {
  assert (cfg.receiver_proc() == _pid);

  _event_loop.run();

  std::cerr << "BYE LOL" << std::endl;
}

void Process::sender_deliver_callback(const std::vector<uint8_t>& data) {
  (void) data;
}

void Process::receiver_deliver_callback(const std::vector<uint8_t>& data) {
  // Deserialize the packet.
  Packet packet;
  packet.deserialize(data);

  // Write to the output file.
  _outfile << "d " << packet.pid() << " " << packet.seq_id() << "\n";
}
