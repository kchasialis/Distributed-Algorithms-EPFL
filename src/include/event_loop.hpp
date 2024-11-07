#pragma once

#include <sys/epoll.h>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <atomic>

#define MAX_EVENTS 100

class EventLoop {
private:
    int _epoll_fd;
    int _exit_loop_fd;
    std::atomic<bool> _running;
    std::unordered_map<int, std::function<void(uint32_t)>> _handlers;
    std::mutex _handlers_mutex;

public:
    EventLoop();
    ~EventLoop();
    void add(int fd, uint32_t events, std::function<void(uint32_t)> handler);
    void run();
    void stop();
    void rearm(int fd, uint32_t event) const;
//    void modify(int fd, uint32_t events);
};
