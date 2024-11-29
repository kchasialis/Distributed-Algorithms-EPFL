#include <cstring>
#include <utility>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fstream>
#include <cassert>
#include "process_pl.hpp"
#include "perfect_link.hpp"

Process::Process(uint64_t pid, in_addr_t addr, uint16_t port,
                 const std::vector<Parser::Host>& hosts, const FifoConfig &cfg,
                 const std::string& outfname)
        : _pid(pid), _addr(addr), _port(port), _hosts(hosts), _outfile(outfname, std::ios::out | std::ios::trunc),
          _n_messages(cfg.num_messages() * (_hosts.size() - 1)) {

  std::cerr << "Expecting " << _n_messages << " messages" << std::endl;

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

  _thread_pool = new ThreadPool(8);

  for (uint32_t i = 0; i < event_loop_workers; i++) {
    _thread_pool->enqueue([this] {
      this->_event_loop.run();
    });
  }
}

Process::~Process() {
  std::cerr << "Goodbye from process " << _pid << std::endl;
  _thread_pool->stop();
  _outfile.close();
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
  {
    std::lock_guard<std::mutex> lock(_outfile_mutex);
    std::cerr << "Flushing output file" << std::endl;
    _outfile.flush();
  }
  _pl->stop();
  _event_loop.stop();
  _stop.store(true);
  _stop_cv.notify_all();
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

  _pl->send(cfg.num_messages(), cfg.receiver_proc(), _outfile, _outfile_mutex);

  // Wait until stop is called.
  {
    std::unique_lock<std::mutex> syn_lock(_stop_mutex);
    _stop_cv.wait(syn_lock, [this] { return _stop.load(); });
  }
}

void Process::run_receiver(const Config& cfg) {
  assert (cfg.receiver_proc() == _pid);

  _pl->send_syn_packets();

  _event_loop.run();
}

void Process::sender_deliver_callback(const Packet& pkt) {
  (void) pkt;
}

// Specialize this function for message data types.
void Process::receiver_deliver_callback(const Packet& pkt) {
  std::lock_guard<std::mutex> lock(_outfile_mutex);
  for (size_t i = 0; i < pkt.data().size(); i += sizeof(uint32_t)) {
    uint32_t seq_id;
    std::memcpy(&seq_id, pkt.data().data() + i, sizeof(uint32_t));
    _outfile << "d " << pkt.pid() << " " << seq_id << "\n";
    assert(_n_messages > 0);
    --_n_messages;
    if (_n_messages == 0) {
      std::cerr << "Process " << _pid << " received all messages!" << std::endl;
    }
  }
//  _outfile.flush();
}
