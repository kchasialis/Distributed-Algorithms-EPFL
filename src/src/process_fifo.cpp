#include <cstring>
#include <utility>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fstream>
#include <cassert>
#include "process_fifo.hpp"
#include "perfect_link.hpp"

ProcessFifo::ProcessFifo(uint64_t pid, in_addr_t addr, uint16_t port,
                         const std::vector<Parser::Host>& hosts, const FifoConfig &cfg,
                         const std::string& outfname)
        : _pid(pid), _addr(addr), _port(port), _outfile(outfname, std::ios::out | std::ios::trunc),
          _n_delivered_messages(cfg.num_messages() * (hosts.size())) {

  std::cerr << "Expecting " << _n_delivered_messages << " messages" << std::endl;

  _thread_pool = new ThreadPool(8);

  _urb = new Urb(pid, addr, port, hosts, _event_loop, _thread_pool,
                 [this](const Packet& pkt) {
                   this->fifo_deliver(pkt);
                 });

  for (uint32_t i = 0; i < event_loop_workers; i++) {
    _thread_pool->enqueue([this] {
        this->_event_loop.run();
    });
  }
}

ProcessFifo::~ProcessFifo() {
  std::cerr << "Goodbye from process " << _pid << std::endl;
  _thread_pool->stop();
  _outfile.close();
  delete _thread_pool;
  delete _urb;
}

uint64_t ProcessFifo::pid() const {
  return _pid;
}

void ProcessFifo::stop() {
  {
    std::lock_guard<std::mutex> lock(_outfile_mutex);
    std::cerr << "Flushing output file" << std::endl;
    _outfile.flush();
  }
  _stop.store(true);
  _urb->stop();
  _event_loop.stop();
  _stop_cv.notify_all();
}

EventLoop& ProcessFifo::event_loop() {
  return _event_loop;
}

void ProcessFifo::run(const FifoConfig& cfg) {
  std::vector<Packet> packets;

  {
    std::lock_guard lock(_outfile_mutex);
    uint32_t current_seq_id = 1;
    uint32_t n_messages = cfg.num_messages();
    for (uint32_t i = 0; i < n_messages; i += 8) {
      uint32_t packet_size = std::min(8U, n_messages - i);
      std::vector<uint8_t> data(packet_size * sizeof(uint32_t));
      for (uint32_t j = 0; j < packet_size; j++) {
        std::memcpy(data.data() + j * sizeof(uint32_t), &current_seq_id,
                    sizeof(uint32_t));
        _outfile << "b " + std::to_string(current_seq_id) + "\n";
        current_seq_id++;
      }
      Packet pkt(_pid, PacketType::DATA, (i / 8) + 1, data);
      packets.push_back(pkt);
    }
    _outfile.flush();
  }

  _urb->broadcast(packets);

  _event_loop.run();

  // Wait until stop is called.
//  {
//    std::unique_lock<std::mutex> syn_lock(_stop_mutex);
//    _stop_cv.wait(syn_lock, [this] { return _stop.load(); });
//  }
}

void ProcessFifo::fifo_deliver(const Packet& pkt) {
  std::cerr << "Process " << _pid << " delivered packet " << pkt.seq_id() << " from " << pkt.pid() << std::endl;
  std::lock_guard<std::mutex> lock(_outfile_mutex);
  for (size_t i = 0; i < pkt.data().size(); i += sizeof(uint32_t)) {
    uint32_t seq_id;
    std::memcpy(&seq_id, pkt.data().data() + i, sizeof(uint32_t));
    _outfile << "d " << pkt.pid() << " " << seq_id << "\n";
    if (_n_delivered_messages == 0) {
      std::cerr << "FATAL: Process " << _pid << " received more messages than expected!" << std::endl;
      exit(1);
    }
    --_n_delivered_messages;
    if (_n_delivered_messages == 0) {
      std::cerr << "Process " << _pid << " received all messages!" << std::endl;
    }
  }
//  _outfile.flush();
}
