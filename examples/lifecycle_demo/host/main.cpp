// Lifecycle demo — opt-in init/shutdown hooks and configuration for plugins.
//
// The host loads a CountingWorker plugin, detects IConfigurable and
// ILifecycleAware via dynamic_cast, and calls them in the correct order:
//   construct → configure() → on_init() → use → on_shutdown() → destroy
//
// Host flow:
//   1. Load plugin with PluginLoader<IWorker>
//   2. Detect IConfigurable → call configure()
//   3. Detect ILifecycleAware → call on_init()
//   4. Process several items, print results
//   5. Detect ILifecycleAware → call on_shutdown()

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "IConfigurable.hpp"
#include "ILifecycleAware.hpp"
#include "IWorker.hpp"
#include "PluginLoader.hpp"
#include "platform/shared_lib.hpp"

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
  fs::path plugin_dir = fs::path(argv[0]).parent_path();
  if (plugin_dir.empty()) plugin_dir = fs::current_path();
  if (argc > 1) {
    plugin_dir = argv[1];
  }

  // --- Step 1: Load the plugin ---
  auto worker_path = plugin_arch::platform::find_plugin(plugin_dir, "counting_worker");
  if (worker_path.empty()) {
    std::cerr << "counting_worker plugin not found in: " << plugin_dir << "\n";
    return 1;
  }

  plugin_arch::PluginLoader<examples::IWorker> loader(worker_path.string());
  auto worker = loader.get_instance();
  std::cout << "Loaded: " << worker->name() << " v" << worker->version()
            << "\n";

  // --- Step 2: Detect IConfigurable and call configure() ---
  auto* configurable = dynamic_cast<plugin_arch::IConfigurable*>(worker.get());

  std::cout << "\nConfiguration check: ";
  if (configurable) {
    std::cout << "IConfigurable detected\n";
    plugin_arch::PluginConfig config = {{"prefix", "[PROCESSED] "}};
    std::cout << "Calling configure()...\n";
    configurable->configure(config);
  } else {
    std::cout << "not configurable, skipping\n";
  }

  // --- Step 3: Detect ILifecycleAware and call on_init() ---
  auto* lifecycle = dynamic_cast<plugin_arch::ILifecycleAware*>(worker.get());

  std::cout << "\nLifecycle check: ";
  if (lifecycle) {
    std::cout << "ILifecycleAware detected\n";
    std::cout << "Calling on_init()...\n";
    lifecycle->on_init();
  } else {
    std::cout << "not lifecycle-aware, skipping init\n";
  }

  // --- Step 4: Process items ---
  std::cout << "\nProcessing items:\n";
  std::vector<std::string> items = {"hello", "world", "plugins"};
  for (const auto& item : items) {
    std::string result = worker->process(item);
    std::cout << "  \"" << item << "\"   -> \"" << result << "\"\n";
  }

  // --- Step 5: Detect ILifecycleAware and call on_shutdown() ---
  if (lifecycle) {
    std::cout << "\nCalling on_shutdown()...\n";
    lifecycle->on_shutdown();
  }

  return 0;
}
