#pragma once

#include <memory>
#include <unordered_map>
#include "parser.hpp"
#include "perfect_link.hpp"
#include "packet.hpp"
#include "config.hpp"
#include "event_loop.hpp"
#include "thread_pool.hpp"

constexpr uint32_t event_loop_workers = 7;

struct pending_t {
    Packet pkt;
    uint64_t peer;
};

struct PendingEqual {
    bool operator()(const pending_t& lhs, const pending_t& rhs) const {
        return lhs.pkt == rhs.pkt && lhs.peer == rhs.peer;
    }
};

struct PendingHash {
    std::size_t operator()(const pending_t& p) const {
      PacketHash hash_pkt;
      std::hash<uint64_t> hash_pid;

      std::size_t seed = hash_pkt(p.pkt);
      seed ^= hash_pid(p.peer) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
      return seed;
    }
};

class ProcessFifo {
public:
    ProcessFifo(uint64_t pid, in_addr_t addr, uint16_t port,
                const std::vector<Parser::Host> &hosts, const FifoConfig& cfg,
                const std::string& outfname);
    ~ProcessFifo();

    uint64_t pid() const;
    void run(const FifoConfig& cfg);
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
    std::unordered_map<Packet, std::unordered_set<uint64_t>, PacketHash, PacketEqual> _ack_proc_map;
    std::mutex _ack_proc_map_mutex;
    std::unordered_set<pending_t, PendingHash, PendingEqual> _pending;
    std::mutex _pending_mutex;
    std::unordered_set<delivered_t, DeliveredHash, DeliveredEqual> _delivered;
    std::mutex _delivered_mutex;

    void broadcast(const std::vector<Packet>& packets);
    void deliver_callback(const Packet& pkt);
    bool can_deliver(const Packet& pkt);
    void do_deliver(const Packet& pkt);
    void monitor_deliver();
};
