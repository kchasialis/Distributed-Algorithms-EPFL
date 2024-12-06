#pragma once

#include <vector>
#include <cstdint>
#include "parser.hpp"
#include "event_loop.hpp"
#include "thread_pool.hpp"
#include "config.hpp"
#include "perfect_link.hpp"

class Urb {
public:
    Urb(uint64_t pid, in_addr_t addr, uint16_t port,
        const std::vector<Parser::Host>& hosts,
        EventLoop &read_event_loop, EventLoop &write_event_loop,
        ThreadPool *_thread_pool, DeliverCallback deliver_cb);
    ~Urb();

//    void broadcast(const std::vector<Packet>& packets);
    void broadcast(std::vector<Packet>& packets);
    void stop();
private:
    struct PendingEqual {
        bool operator()(const Packet& lhs, const Packet& rhs) const {
          return lhs.seq_id() == rhs.seq_id() && lhs.pid() == rhs.pid();
        }
    };

    struct PendingHash {
        std::size_t operator()(const Packet& pkt) const {
          std::hash<uint32_t> hash_seq_id;
          std::hash<uint64_t> hash_pid;

          std::size_t seed = hash_seq_id(pkt.seq_id());
          seed ^= hash_pid(pkt.pid()) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
          return seed;
        }
    };

    uint64_t _pid;
    in_addr_t _addr;
    uint16_t _port;
    PerfectLink *_pl;
    std::vector<Parser::Host> _hosts;

//    std::array<std::unordered_map<Packet, std::unordered_set<uint64_t>, PacketHash, PacketEqual>, MAX_PROCESSES> _ack_proc_maps;
//    std::array<std::mutex, MAX_PROCESSES> _ack_proc_map_mutexes;

    std::vector<std::unordered_set<Packet, PendingHash, PendingEqual>> _pending_sets;
    std::vector<std::mutex> _pending_mutexes;

//    std::unordered_map<Packet, std::unordered_set<uint64_t>, PacketHash, PacketEqual> _ack_proc_map;
    std::unordered_map<uint32_t, std::unordered_set<uint64_t>> _ack_proc_map;
    std::mutex _ack_proc_map_mutex;
//
//    std::unordered_set<Packet, PendingHash, PendingEqual> _pending;
//    std::mutex _pending_mutex;

    std::atomic<bool> _stop;
    DeliverCallback _deliver_cb;

//    void beb_broadcast(const std::vector<Packet>& packets);
    void beb_broadcast(std::vector<Packet>& packets);
    void beb_deliver(Packet &&pkt);
//    void beb_deliver(const Packet& pkt);
    bool can_deliver(const Packet& pkt);
    void do_deliver(Packet &&pkt);
//    void do_deliver(const Packet& pkt);
    void monitor_delivery(size_t thread_id, size_t total_threads);
//    void monitor_delivery();
};
