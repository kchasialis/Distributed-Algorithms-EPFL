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
        : _pid(pid), _addr(addr), _port(port), _hosts(hosts), _outfile(outfname, std::ios::out | std::ios::trunc),
          _n_messages(cfg.num_messages() * (_hosts.size() - 1)) {

  std::cerr << "Expecting " << _n_messages << " messages" << std::endl;

  _pl = new PerfectLink(pid, _addr, _port, _hosts, _event_loop,
                        [this](const Packet& pkt) {
              this->deliver_callback(pkt);
          });

  _thread_pool = new ThreadPool(8);

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
  delete _pl;
  delete _thread_pool;
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
  _pl->stop();
  _event_loop.stop();
  _stop.store(true);
  _stop_cv.notify_all();
}

EventLoop& ProcessFifo::event_loop() {
  return _event_loop;
}

void ProcessFifo::run(const FifoConfig& cfg) {
  std::vector<Packet> packets;

  uint32_t current_seq_id = 1;
  uint32_t n_messages = cfg.num_messages();
  for (uint32_t i = 0; i < n_messages; i += 8) {
    uint32_t packet_size = std::min(8U, n_messages - i);
    std::vector<uint8_t> data(packet_size * sizeof(uint32_t));
    for (uint32_t j = 0; j < packet_size; j++) {
      std::memcpy(data.data() + j * sizeof(uint32_t), &current_seq_id,
                  sizeof(uint32_t));
      current_seq_id++;
    }

    Packet pkt(_pid, PacketType::DATA, (i / 8) + 1, data);
    packets.push_back(pkt);
  }

  {
    std::lock_guard<std::mutex> lock(_pending_mutex);
    for (const auto& pkt : packets) {
      _pending.insert({pkt, _pid});
    }
  }

  broadcast(packets);

  // Wait until stop is called.
  {
    std::unique_lock<std::mutex> syn_lock(_stop_mutex);
    _stop_cv.wait(syn_lock, [this] { return _stop.load(); });
  }
}

void ProcessFifo::broadcast(const std::vector<Packet>& packets) {
  // Deliver packets to self.
  for (const auto& pkt : packets) {
    deliver_callback(pkt);
  }

  for (const auto& host : _hosts) {
    if (host.id != _pid) {
      std::cerr << "Process " << _pid << " sending packets to " << host.id << std::endl;
      _pl->send(packets, host.id);
      std::cerr << "Process " << _pid << " sent packets to " << host.id << std::endl;
    }
  }
}

void ProcessFifo::deliver_callback(const Packet& pkt) {
  std::cerr << "Process " << _pid << " received packet " << pkt.seq_id() << " from " << pkt.pid() << std::endl;
  {
    std::lock_guard<std::mutex> lock(_ack_proc_map_mutex);
    auto it = _ack_proc_map.find(pkt);
    if (it == _ack_proc_map.end()) {
      _ack_proc_map[pkt] = std::unordered_set<uint64_t>();
    }
    _ack_proc_map[pkt].insert(pkt.pid());
  }

  if (can_deliver(pkt)) {
    do_deliver(pkt);
  }

//  {
//    std::lock_guard<std::mutex> lock(_pending_mutex);
////    pending_t pending_pkt = {pkt, pkt.pid()};
////    if (_pending.find(pending_pkt) == _pending.end()) {
////      _pending.insert(pending_pkt);
////      std::vector<Packet> packets;
////      packets.push_back(pkt);
////      broadcast(packets);
////    }
//
//    for (auto it = _pending.begin(); it != _pending.end();) {
//      if (can_deliver(it->pkt)) {
//        do_deliver(it->pkt);
//        it = _pending.erase(it);
//      } else {
//        std::cerr << "Process " << _pid << " cannot deliver packet " << it->pkt.seq_id() << std::endl;
//        ++it;
//      }
//    }
//  }
}

bool ProcessFifo::can_deliver(const Packet& pkt) {
  std::lock_guard<std::mutex> lock(_ack_proc_map_mutex);
  auto it = _ack_proc_map.find(pkt);
  if (it == _ack_proc_map.end()) {
    return false;
  }
  return it->second.size() > _hosts.size() / 2;
}

void ProcessFifo::do_deliver(const Packet& pkt) {
  std::cerr << "Process " << _pid << " delivered packet " << pkt.seq_id() << std::endl;
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
