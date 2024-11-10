#include "thread_pool.hpp"

ThreadPool::ThreadPool(size_t num_threads) : _stop_threads(false) {
  for (size_t i = 0; i < num_threads; ++i) {
    _workers.emplace_back([this] { worker_function(); });
  }
}

void ThreadPool::stop() {
  _stop_threads = true;
  cv.notify_all();
  for (std::thread &worker : _workers) {
    if (worker.joinable())
      worker.join();
  }
}

void ThreadPool::enqueue(std::function<void()> task) {
  {
    std::unique_lock<std::mutex> lock(_queue_mutex);
    if (_stop_threads) {
      throw std::runtime_error("enqueue on stopped ThreadPool");
    }
    _task_queue.emplace(std::move(task));
  }
  cv.notify_one();
}

void ThreadPool::worker_function() {
  while (true) {
    std::function<void()> task;

    {
      std::unique_lock<std::mutex> lock(_queue_mutex);
      cv.wait(lock, [this] { return _stop_threads || !_task_queue.empty(); });

      if (_stop_threads && _task_queue.empty())
        return;

      task = std::move(_task_queue.front());
      _task_queue.pop();
    }

    task();
  }
}
