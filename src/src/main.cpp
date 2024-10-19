#include <chrono>
#include <iostream>
#include <thread>
#include <csignal>

#include "parser.hpp"
#include "process.hpp"
#include "config.hpp"

static void stop(int) {
  // reset signal handlers to default
  signal(SIGTERM, SIG_DFL);
  signal(SIGINT, SIG_DFL);

  // immediately stop network packet processing
  std::cout << "Immediately stopping network packet processing.\n";

  // write/flush output file if necessary
  std::cout << "Writing output.\n";

  // exit directly from signal handler
  exit(0);
}

static void register_signals() {
  // register signal handlers
  signal(SIGTERM, stop);
  signal(SIGINT, stop);
}

static Config read_config_file(const std::string &configPath) {
  std::ifstream config_file(configPath);
  if (!config_file) {
    std::cerr << "Failed to open config file: " << configPath << std::endl;
    exit(1);
  }

  uint32_t n_messages, sender_proc;
  if (!(config_file >> n_messages >> sender_proc)) {
    std::cerr << "Failed to read config values from: " << configPath << std::endl;
    exit(1);
  }

  return {n_messages, sender_proc};
}

static int run_process(Parser &parser, const Config& cfg) {
  std::vector<Process> processes;
  for (const auto &host: parser.hosts()) {
    processes.emplace_back(host.id, host.ip, host.port, cfg.sender_proc() == host.id);
  }

  for (const auto &process: processes) {
    std::cout << "Process " << process.id() << " is a sender: " << process.sender() << std::endl;
  }

  return 0;
}

int main(int argc, char **argv) {
  register_signals();
  Parser parser(argc, argv);
  parser.parse();
  Config cfg = read_config_file(parser.configPath());

  int err = run_process(parser, cfg);
  if (err != 0) {
    return err;
  }

  // `true` means that a config file is required.
  // Call with `false` if no config file is necessary.
  bool requireConfig = true;

  // After a process finishes broadcasting,
  // it waits forever for the delivery of messages.
  while (true) {
    std::this_thread::sleep_for(std::chrono::hours(1));
  }

  return 0;
}
