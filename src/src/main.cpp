#include <chrono>
#include <iostream>
#include <thread>
#include <csignal>
#include <mutex>
#include <functional>

#include "config.hpp"
#include "parser.hpp"
#include "process_lattice.hpp"

std::mutex signal_handler_mutex;
std::function<void()> signal_handler;

static void stop(int) {
  // reset signal handlers to default
  signal(SIGTERM, SIG_DFL);
  signal(SIGINT, SIG_DFL);

  signal_handler();

//  std::cerr << "Stop requested" << std::endl;

  // exit directly from signal handler
//  exit(0);
}

static void register_signals() {
  // register signal handlers
  signal(SIGTERM, stop);
  signal(SIGINT, stop);
}


static int run_process(Parser &parser, const LatticeConfig& cfg) {
  Parser::Host current_host;
  bool found = false;
  auto hosts = parser.hosts();
  for (const auto &host: hosts) {
    if (host.id == parser.id()) {
      current_host = host;
      found = true;
      break;
    }
  }
  if (!found) {
    std::cerr << "Failed to find host with id: " << parser.id() << std::endl;
    return 1;
  }

  std::cerr << "Starting process with id: " << parser.id() << std::endl;

  ProcessLattice process(parser.id(), current_host.ip, current_host.port,
                         hosts, cfg, parser.outputPath());

  signal_handler = [&process]() { process.stop(); };

  process.run();

  return 0;
}

int main(int argc, char **argv) {
  register_signals();
  Parser parser(argc, argv);
  parser.parse();
  LatticeConfig lattice_cfg(parser.configPath());

  int err = run_process(parser, lattice_cfg);
  if (err != 0) {
    std::cerr << "Failed to run process with id: " << parser.id() << std::endl;
    return err;
  }

  // `true` means that a config file is required.
  // Call with `false` if no config file is necessary.
//  bool requireConfig = true;

//   After a process finishes broadcasting,
//   it waits forever for the delivery of messages.
//  while (true) {
//    std::this_thread::sleep_for(std::chrono::hours(1));
//  }

  return 0;
}
