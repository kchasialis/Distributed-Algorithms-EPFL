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
        : _pid(pid), _addr(addr), _port(port), _pending(hosts.size()),
          _next(hosts.size()), _outfile(outfname, std::ios::out | std::ios::trunc),
          _n_delivered_messages(cfg.num_messages() * (hosts.size())) {

  _start_time = std::chrono::steady_clock::now();

  for (const auto& host : hosts) {
    size_t index = host.id - 1;
    _next[index] = 1;
    _pending[index].reserve(_n_delivered_messages);
  }

  _thread_pool = new ThreadPool(8);

//  _urb = new Urb(pid, addr, port, hosts, _read_event_loop, _write_event_loop,
//                 _thread_pool, [this](const Packet& pkt) {
//                   this->urb_deliver(pkt);
//                 });

  _urb = new Urb(pid, addr, port, hosts, _read_event_loop, _write_event_loop,
                 _thread_pool, [this](Packet &&pkt) {
              this->urb_deliver(std::move(pkt));
          });

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
    _outfile.flush();
  }
  _stop.store(true);
  _urb->stop();
  _write_event_loop.stop();
  _read_event_loop.stop();
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

  _read_event_loop.run();
}

//void ProcessFifo::urb_deliver(const Packet &pkt) {
//  std::vector<Packet> to_deliver;
//  {
//    std::lock_guard<std::mutex> lock(_pending_mutex);
//    _pending[pkt.pid()].insert(pkt);
//
//    auto &packets = _pending[pkt.pid()];
//    for (auto it = packets.begin(); it != packets.end();) {
//      if (it->seq_id() == _next[pkt.pid()]) {
//        to_deliver.push_back(*it);
//        it = packets.erase(it);
//        _next[pkt.pid()]++;
//      } else {
//        ++it;
//      }
//    }
//  }
//
//  for (const auto &d_pkt: to_deliver) {
//    fifo_deliver(d_pkt);
//  }
//}

void ProcessFifo::urb_deliver(Packet &&pkt) {
  std::vector<Packet> to_deliver;

  size_t index = pkt.pid() - 1;

  _pending[index].push_back(pkt);
//  _pending[index].push(pkt);
  auto &packets = _pending[index];
  to_deliver.reserve(packets.size());

  while (true) {
    bool found = false;
    for (auto it = packets.begin(); it != packets.end();) {
      if (it->seq_id() == _next[index]) {
        to_deliver.push_back(std::move(*it));
//        it = packets.erase(it);
        _next[index]++;
        found = true;
      } else {
        ++it;
      }
    }
    if (!found) {
      break;
    }
  }

//  for (auto it = packets.begin(); it != packets.end();) {
//    if (it->seq_id() == _next[index]) {
//      to_deliver.push_back(*it);
//      it = packets.erase(it);
//      _next[index]++;
//    } else {
//      ++it;
//    }
//  }


  fifo_deliver_all(to_deliver);
}

//void ProcessFifo::urb_deliver(const Packet &pkt) {
//  std::vector<Packet> to_deliver;
//
//  size_t index = pkt.pid() - 1;
//
//  _pending[index].push_back(pkt);
////  _pending[index].push(pkt);
//  auto &packets = _pending[index];
//  to_deliver.reserve(packets.size());
//
//  while (true) {
//    bool found = false;
//    for (auto it = packets.begin(); it != packets.end();) {
//      if (it->seq_id() == _next[index]) {
//        to_deliver.push_back(std::move(*it));
////        it = packets.erase(it);
//        _next[index]++;
//        found = true;
//      } else {
//        ++it;
//      }
//    }
//    if (!found) {
//      break;
//    }
//  }
//
////  for (auto it = packets.begin(); it != packets.end();) {
////    if (it->seq_id() == _next[index]) {
////      to_deliver.push_back(*it);
////      it = packets.erase(it);
////      _next[index]++;
////    } else {
////      ++it;
////    }
////  }
//
//
//  fifo_deliver_all(to_deliver);
//}

void ProcessFifo::fifo_deliver_all(const std::vector<Packet>& packets) {
  std::lock_guard<std::mutex> lock(_outfile_mutex);
  for (const auto &pkt: packets) {
    fifo_deliver(pkt);
  }
}

void ProcessFifo::fifo_deliver(const Packet& pkt) {
//  std::lock_guard<std::mutex> lock(_outfile_mutex);
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
      auto end_time = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - _start_time).count();
      std::cerr << "Process " << _pid << " received all messages! Execution time: "
                << duration << " ms" << std::endl;
    }
  }
}
