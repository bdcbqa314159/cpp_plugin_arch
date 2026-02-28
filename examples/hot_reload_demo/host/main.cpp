#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <thread>

#include "HotPluginLoader.hpp"
#include "IGreeter.hpp"

namespace fs = std::filesystem;

static volatile std::sig_atomic_t running = 1;

static void signal_handler(int) { running = 0; }

static std::string plugin_extension() {
#if defined(_WIN32)
  return ".dll";
#elif defined(__APPLE__)
  return ".dylib";
#else
  return ".so";
#endif
}

static fs::path find_plugin(const fs::path& dir, const std::string& name) {
  std::string ext = plugin_extension();
  for (const auto& entry : fs::directory_iterator(dir)) {
    auto filename = entry.path().filename().string();
    if (entry.path().extension() == ext &&
        filename.find(name) != std::string::npos) {
      return entry.path();
    }
  }
  throw std::runtime_error("Plugin not found: " + name + " in " + dir.string());
}

int main(int argc, char* argv[]) {
  fs::path plugin_dir = fs::path(argv[0]).parent_path();
  if (argc > 1) {
    plugin_dir = argv[1];
  }

  auto greeter_path = find_plugin(plugin_dir, "greeter");

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
