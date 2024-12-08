#pragma once

#include <memory>
#include <map>
#include <unordered_map>
#include <queue>
#include "parser.hpp"
#include "perfect_link.hpp"
#include "packet.hpp"
#include "config.hpp"
#include "event_loop.hpp"
#include "thread_pool.hpp"
#include "urb.hpp"

class ProcessFifo {
public:
    ProcessFifo(uint64_t pid, in_addr_t addr, uint16_t port,
                const std::vector<Parser::Host> &hosts, const FifoConfig& cfg,
                const std::string& outfname);
    ~ProcessFifo();

    uint64_t pid() const;
    void run(const FifoConfig& cfg);
    void stop();
private:
    uint64_t _pid;
    in_addr_t _addr;
    uint16_t _port;
    EventLoop _read_event_loop;
    EventLoop _write_event_loop;
    ThreadPool *_thread_pool;
    Urb *_urb;

    std::vector<std::vector<Packet>> _pending;
    std::vector<std::atomic<uint32_t>> _next;

    std::mutex _outfile_mutex;
    std::ofstream _outfile;
    size_t _n_delivered_messages;
    std::atomic<bool> _stop{false};

    std::chrono::steady_clock::time_point _start_time;

    void urb_deliver(Packet &&pkt);
    void fifo_deliver_all(const std::vector<Packet>& packets);
    void fifo_deliver(const Packet& pkt);
};
