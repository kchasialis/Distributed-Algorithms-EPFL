#pragma once

#include <sys/epoll.h>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <atomic>

#define MAX_EVENTS 100

struct EventData {
    int fd;
    void *handler_obj;
};

class EventLoop {
private:
    int _epoll_fd;
    int _exit_loop_fd;
    std::atomic<bool> _running;
    EventData _exit_loop_data{};

public:
    EventLoop();
    ~EventLoop();
    void add(uint32_t events, EventData *event_data) const;
    void rearm(uint32_t event, EventData *event_data) const;
    void run();
    void stop();
};
