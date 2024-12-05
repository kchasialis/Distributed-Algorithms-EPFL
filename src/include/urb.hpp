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

    void broadcast(const std::vector<Packet>& packets);
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
    std::unordered_map<Packet, std::unordered_set<uint64_t>, PacketHash, PacketEqual> _ack_proc_map;
    std::mutex _ack_proc_map_mutex;
    std::unordered_set<Packet, PendingHash, PendingEqual> _pending;
    std::mutex _pending_mutex;
    std::unordered_set<delivered_t, DeliveredHash, DeliveredEqual> _delivered;
    std::mutex _delivered_mutex;
    std::atomic<bool> _stop;
    DeliverCallback _deliver_cb;

    void beb_broadcast(const std::vector<Packet>& packets);
    void beb_deliver(const Packet& pkt);
    bool can_deliver(const Packet& pkt);
    void do_deliver(const Packet& pkt);
    void monitor_delivery();
};
