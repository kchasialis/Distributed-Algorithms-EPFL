#pragma once

#include <sys/epoll.h>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <atomic>

#define MAX_EVENTS 100

struct EventData {
    int fd;
    uint32_t events;
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
    void add(EventData *event_data) const;
    void rearm(EventData *event_data) const;
    void run();
    void stop();
};
