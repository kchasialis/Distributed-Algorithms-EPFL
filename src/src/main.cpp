#include <chrono>
#include <iostream>
#include <thread>
#include <csignal>

#include "config.hpp"
#include "parser.hpp"
#include "process_fifo.hpp"

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

static PlConfig read_config_file_pl(const std::string &configPath) {
  std::ifstream config_file(configPath);
  if (!config_file) {
    std::cerr << "Failed to open config file: " << configPath << std::endl;
    exit(1);
  }

  uint32_t n_messages, receiver_proc;
  if (!(config_file >> n_messages >> receiver_proc)) {
    std::cerr << "Failed to read config values from: " << configPath << std::endl;
    exit(1);
  }

  return {n_messages, receiver_proc};
}

static FifoConfig read_config_file_fifo(const std::string &configPath) {
  std::ifstream config_file(configPath);
  if (!config_file) {
    std::cerr << "Failed to open config file: " << configPath << std::endl;
    exit(1);
  }

  uint32_t n_messages;
  if (!(config_file >> n_messages)) {
    std::cerr << "Failed to read config values from: " << configPath << std::endl;
    exit(1);
  }

  return {n_messages};
}

static int run_process(Parser &parser, const FifoConfig& cfg) {
  Parser::Host current_host;
  bool found = false;
  for (const auto &host: parser.hosts()) {
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

  std::cerr << "I am process with id: " << parser.id() << std::endl;

  ProcessFifo process(parser.id(), current_host.ip, current_host.port,
                      parser.hosts(), cfg, parser.outputPath());

//  {
//    std::lock_guard<std::mutex> lock(signal_handler_mutex);
//    signal_handler = [&process]() { process.stop(); };
//  }

  signal_handler = [&process]() { process.stop(); };

  process.run(cfg);
//  process.stop();

  return 0;
}

int main(int argc, char **argv) {
  register_signals();
  Parser parser(argc, argv);
  parser.parse();
  FifoConfig cfg = read_config_file_fifo(parser.configPath());

  int err = run_process(parser, cfg);
  if (err != 0) {
    std::cerr << "Failed to run process with id: " << parser.id() << std::endl;
    return err;
  }

  // `true` means that a config file is required.
  // Call with `false` if no config file is necessary.
  bool requireConfig = true;

//   After a process finishes broadcasting,
//   it waits forever for the delivery of messages.
//  while (true) {
//    std::this_thread::sleep_for(std::chrono::hours(1));
//  }

  return 0;
}
