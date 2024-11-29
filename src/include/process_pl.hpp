#pragma once

#include <memory>
#include <unordered_map>
#include "parser.hpp"
#include "perfect_link.hpp"
#include "packet.hpp"
#include "config.hpp"
#include "event_loop.hpp"
#include "thread_pool.hpp"

constexpr uint32_t event_loop_workers = 10;

class Process {
public:
  Process(uint64_t pid, in_addr_t addr, uint16_t port,
          const std::vector<Parser::Host> &hosts, const Config& cfg,
          const std::string& outfname);
  ~Process();

  uint64_t pid() const;
  void run(const Config& cfg);
  void stop();
  EventLoop& event_loop();
private:
    uint64_t _pid;
    in_addr_t _addr;
    uint16_t _port;
    EventLoop _event_loop;
    ThreadPool *_thread_pool;
    std::vector<Parser::Host> _hosts;
    PerfectLink *_pl;
    std::mutex _outfile_mutex;
    std::ofstream _outfile;
    size_t _n_messages;
    std::atomic<bool> _stop{false};
    std::mutex _stop_mutex;
    std::condition_variable _stop_cv;

    void run_sender(const Config& cfg);
    void run_receiver(const Config& cfg);
    static void sender_deliver_callback(const Packet& pkt);
    void receiver_deliver_callback(const Packet& pkt);
};

