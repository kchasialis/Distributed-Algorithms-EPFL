#pragma once

#include <memory>
#include <unordered_map>
#include "parser.hpp"
#include "perfect_link.hpp"
#include "packet.hpp"
#include "config.hpp"
#include "event_loop.hpp"
#include "thread_pool.hpp"
#include "urb.hpp"

//constexpr uint32_t event_loop_workers = 16;
constexpr uint32_t read_event_loop_workers = 5;
constexpr uint32_t write_event_loop_workers = 2;

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
    std::mutex _pending_next_mutex;
    std::unordered_map<uint64_t, std::set<Packet, PacketLess>> _pending; // pid -> packets
    std::unordered_map<uint64_t, uint32_t> _next; // pid -> seq_id
    std::mutex _outfile_mutex;
    std::ofstream _outfile;
    size_t _n_delivered_messages;
    std::atomic<bool> _stop{false};

    std::chrono::steady_clock::time_point _start_time;

    void urb_deliver(const Packet& pkt);
    void fifo_deliver(const Packet& pkt);
};
