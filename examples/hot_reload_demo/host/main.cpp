#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <thread>

#include "HotPluginLoader.hpp"
#include "IGreeter.hpp"
#include "platform/shared_lib.hpp"

namespace fs = std::filesystem;

static volatile std::sig_atomic_t running = 1;

static void signal_handler(int) { running = 0; }

int main(int argc, char* argv[]) {
  fs::path plugin_dir = fs::path(argv[0]).parent_path();
  if (argc > 1) {
    plugin_dir = argv[1];
  }

  auto greeter_path = plugin_arch::platform::find_plugin(plugin_dir, "greeter");
  if (greeter_path.empty()) {
    std::cerr << "greeter plugin not found in: " << plugin_dir << "\n";
    return 1;
  }

  plugin_arch::HotPluginLoader<examples::IGreeter> loader(greeter_path.string());
  auto greeter = loader.get_instance();

  std::cout << "[" << greeter->name() << " v" << greeter->version() << "] loaded\n";
  std::cout << "> " << greeter->greet() << "\n\n";

  std::cout << "Polling for changes every 2 seconds (Ctrl-C to exit)...\n\n";

  std::signal(SIGINT, signal_handler);

  while (running) {
    std::this_thread::sleep_for(std::chrono::seconds(2));
    if (!running) break;

    if (loader.check_and_reload()) {
      greeter = loader.get_instance();
      std::cout << "[reloaded] " << greeter->name() << " v" << greeter->version()
                << "\n";
      std::cout << "> " << greeter->greet() << "\n\n";
    }
  }

  std::cout << "\nExiting.\n";
  return 0;
}
