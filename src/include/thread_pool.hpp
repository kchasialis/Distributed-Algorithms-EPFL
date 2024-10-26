#pragma once

#include <iostream>
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>

class ThreadPool {
public:
    ThreadPool(size_t num_threads);

    void stop();
    void enqueue(std::function<void()> task);
private:
    std::vector<std::thread> _workers;
    std::queue<std::function<void()>> _task_queue;
    std::mutex _queue_mutex;
    std::condition_variable cv;
    std::atomic<bool> _stop_threads;

    void worker_function();
};
