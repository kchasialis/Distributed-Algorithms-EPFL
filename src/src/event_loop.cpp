#include <iostream>
#include <utility>
#include <unistd.h>
#include <cassert>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include "event_loop.hpp"
#include "stubborn_link.hpp"

EventLoop::EventLoop() : _running(true) {
  _epoll_fd = epoll_create1(0);
  if (_epoll_fd == -1) {
    perror("epoll_create1 failed");
    exit(EXIT_FAILURE);
  }

  // Create the wakeup event file descriptor
  _exit_loop_fd = eventfd(0, 0);
  if (_exit_loop_fd == -1) {
    perror("eventfd failed");
    close(_epoll_fd);
    exit(EXIT_FAILURE);
  }

  // Add the wakeup file descriptor to epoll for monitoring
  _exit_loop_data.fd = _exit_loop_fd;
  add(EPOLLIN, &_exit_loop_data);
}

EventLoop::~EventLoop() {
  close(_exit_loop_fd);
  close(_epoll_fd);
}

void EventLoop::add(uint32_t events, EventData *event_data) const {
  struct epoll_event ev{};
  ev.events = events | EPOLLONESHOT;
  ev.data.ptr = event_data;
  if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, event_data->fd, &ev) == -1) {
    perror("epoll_ctl failed");
    close(_epoll_fd);
    close(_exit_loop_fd);
    exit(EXIT_FAILURE);
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
void EventLoop::rearm(uint32_t event, EventData *event_data) const {
  epoll_event ev{};
  ev.events = event | EPOLLONESHOT;
  ev.data.ptr = event_data;
  if (epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, event_data->fd, &ev) == -1) {
    perror("epoll_ctl rearm failed");
    close(_epoll_fd);
    close(_exit_loop_fd);
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
      auto *event_data = static_cast<EventData *>(events[i].data.ptr);
      if (events[i].events & EPOLLERR) {
        rearm(EPOLLIN, event_data);
        continue;
      }

      if ((events[i].events & EPOLLIN) && event_data->fd == _exit_loop_fd) {
        uint64_t u;
        // Read to clear the read buffer.
        if (read(_exit_loop_fd, &u, sizeof(u)) == -1) {
          break;
        }
        rearm(EPOLLIN, event_data);

        // Write to wakeup file descriptor to unblock epoll_wait
        u = 1;
        if (write(_exit_loop_fd, &u, sizeof(u)) == -1) {
          perror("write to wakeup_fd failed");
        }
        continue;
      }

      // Call the handler
      auto *handler = static_cast<ReadEventHandler *>(event_data->handler_obj);
      handler->handle_read_event(events[i].events);
      rearm(EPOLLIN, event_data);
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
