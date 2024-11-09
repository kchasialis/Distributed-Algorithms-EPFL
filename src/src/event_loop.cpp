#include <iostream>
#include <utility>
#include <unistd.h>
#include <cassert>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include "event_loop.hpp"

EventLoop::EventLoop() : _running(true) {
  _epoll_fd = epoll_create1(0);
  if (_epoll_fd == -1) {
    perror("epoll_create1 failed");
    exit(EXIT_FAILURE);
  }

  // Create the wakeup event file descriptor
  _exit_loop_fd = eventfd(0, EFD_NONBLOCK);
  if (_exit_loop_fd == -1) {
    perror("eventfd failed");
    close(_epoll_fd);
    exit(EXIT_FAILURE);
  }

  // Add the wakeup file descriptor to epoll for monitoring
  struct epoll_event ev{};
  ev.events = EPOLLIN;
  ev.data.fd = _exit_loop_fd;
  if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _exit_loop_fd, &ev) == -1) {
    perror("epoll_ctl failed");
    close(_epoll_fd);
    close(_exit_loop_fd);
    exit(EXIT_FAILURE);
  }
}

EventLoop::~EventLoop() {
  close(_exit_loop_fd);
  close(_epoll_fd);
}

void EventLoop::add(int fd, uint32_t events, std::function<void(uint32_t)> handler) {
  struct epoll_event ev{};
  ev.events = events | EPOLLONESHOT;
//  ev.events = events;
  ev.data.fd = fd;
  if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1) {
    perror("epoll_ctl failed");
    exit(EXIT_FAILURE);
  }

  {
    std::unique_lock<std::mutex> _handlers_lock(_handlers_mutex);
    _handlers[fd] = std::move(handler);
  }
}

/* From the epoll manual:
 * Since even with edge-triggered epoll (EPOLLET), multiple events can be
 * generated upon receipt of multiple chunks of data, the caller has
 * the option to specify the EPOLLONESHOT flag, to tell epoll to
 * disable the associated file descriptor after the receipt of an
 * event with epoll_wait(2).  When the EPOLLONESHOT flag is
 * specified, it is the caller's responsibility to rearm the file
 * descriptor using epoll_ctl(2) with EPOLL_CTL_MOD.
 * */
void EventLoop::rearm(int fd, uint32_t event) const {
  epoll_event ev{};
  ev.events = event | EPOLLONESHOT;
  ev.data.fd = fd;
  if (epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
    perror("epoll_ctl rearm failed");
    exit(EXIT_FAILURE);
  }
}

void EventLoop::run() {
  struct epoll_event events[MAX_EVENTS];
  while (_running) {
    int nfds = epoll_wait(_epoll_fd, events, MAX_EVENTS, -1);
    if (nfds == -1) {
      if (errno == EINTR) {
        // Received interrupt signal, continue waiting until stop is called.
        continue;
      }
      perror("epoll_wait failed");
      exit(EXIT_FAILURE);
    }
    for (int i = 0; i < nfds; i++) {
      if (events[i].events & EPOLLERR) {
        int err = 0;
        socklen_t len = sizeof(err);
        if (getsockopt(events[i].data.fd, SOL_SOCKET, SO_ERROR, &err, &len) == 0) {
//          std::cerr << "[DEBUG] EPOLLERR on fd: " << events[i].data.fd
//                    << ", error: " << strerror(err) << std::endl;
        }
        if (err == ECONNREFUSED) {
          rearm(events[i].data.fd, EPOLLIN);
        }
        continue;
      }

      if ((events[i].events & EPOLLIN) && events[i].data.fd == _exit_loop_fd) {
//        std::cerr << "[DEBUG] Received exit signal. " << std::endl;
        uint64_t u;
        // Read to clear the read buffer.
        if (read(_exit_loop_fd, &u, sizeof(u)) == -1) {
          break;
//          perror("read from wakeup_fd failed");
//          exit(EXIT_FAILURE);
        }
        rearm(_exit_loop_fd, EPOLLIN);

        // Write to wakeup file descriptor to unblock epoll_wait
        u = 1;
        if (write(_exit_loop_fd, &u, sizeof(u)) == -1) {
          perror("write to wakeup_fd failed");
        }
//        std::cerr << "[DEBUG] Writing exit signal. " << std::endl;
        continue;
      }

      {
        std::unique_lock<std::mutex> _handlers_lock(_handlers_mutex);
        auto it = _handlers.find(events[i].data.fd);
        assert(it != _handlers.end() && "Handler not found!");
        it->second(events[i].events);
      }
    }
  }
}

void EventLoop::stop() {
  _running = false;

  // Write to wakeup file descriptor to unblock epoll_wait
  uint64_t u = 1;
  if (write(_exit_loop_fd, &u, sizeof(u)) == -1) {
    perror("write to wakeup_fd failed");
    exit(EXIT_FAILURE);
  }
}
