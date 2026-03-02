// Lifecycle demo — opt-in init/shutdown hooks for plugins.
//
// The host loads a CountingWorker plugin, detects ILifecycleAware via
// dynamic_cast, and calls on_init() / on_shutdown() at the right times.
//
// Host flow:
//   1. Load plugin with PluginLoader<IWorker>
//   2. Detect ILifecycleAware → call on_init()
//   3. Process several items, print results
//   4. Detect ILifecycleAware → call on_shutdown()

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "ILifecycleAware.hpp"
#include "IWorker.hpp"
#include "PluginLoader.hpp"
#include "platform/shared_lib.hpp"

namespace fs = std::filesystem;

static fs::path find_plugin(const fs::path& dir, const std::string& name) {
  auto ext = plugin_arch::platform::shared_lib_extension();
  for (const auto& entry : fs::directory_iterator(dir)) {
    auto filename = entry.path().filename().string();
    if (entry.path().extension() == ext &&
        filename.find(name) != std::string::npos) {
      return entry.path();
    }
  }
  return {};
}

int main(int argc, char* argv[]) {
  fs::path plugin_dir = fs::path(argv[0]).parent_path();
  if (argc > 1) {
    plugin_dir = argv[1];
  }

  // --- Step 1: Load the plugin ---
  auto worker_path = find_plugin(plugin_dir, "counting_worker");
  if (worker_path.empty()) {
    std::cerr << "counting_worker plugin not found in: " << plugin_dir << "\n";
    return 1;
  }

  plugin_arch::PluginLoader<examples::IWorker> loader(worker_path.string());
  auto worker = loader.get_instance();
  std::cout << "Loaded: " << worker->name() << " v" << worker->version()
            << "\n";

  // --- Step 2: Detect ILifecycleAware and call on_init() ---
  auto* lifecycle = dynamic_cast<plugin_arch::ILifecycleAware*>(worker.get());

  std::cout << "\nLifecycle check: ";
  if (lifecycle) {
    std::cout << "ILifecycleAware detected\n";
    std::cout << "Calling on_init()...\n";
    lifecycle->on_init();
  } else {
    std::cout << "not lifecycle-aware, skipping init\n";
  }

  // --- Step 3: Process items ---
  std::cout << "\nProcessing items:\n";
  std::vector<std::string> items = {"hello", "world", "plugins"};
  for (const auto& item : items) {
    std::string result = worker->process(item);
    std::cout << "  \"" << item << "\"   -> \"" << result << "\"\n";
  }

  // --- Step 4: Detect ILifecycleAware and call on_shutdown() ---
  if (lifecycle) {
    std::cout << "\nCalling on_shutdown()...\n";
    lifecycle->on_shutdown();
  }

  return 0;
}
