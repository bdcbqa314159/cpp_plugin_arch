// Managed demo — PluginManager handles discovery, dependency ordering,
// configuration, service wiring, and lifecycle for all plugins.
//
// Three plugins:
//   Logger     (no deps)      — provides "logger" service
//   Processor  (deps: logger) — provides "processor" service
//   Aggregator (deps: logger, processor) — uses both via service locator
//
// PluginManager topologically sorts them so Logger loads first, Processor
// second, Aggregator last — regardless of discovery order.

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "IAggregator.hpp"
#include "PluginManager.hpp"

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
  fs::path plugin_dir = fs::path(argv[0]).parent_path();
  if (plugin_dir.empty()) plugin_dir = fs::current_path();
  if (argc > 1) {
    plugin_dir = argv[1];
  }

  // --- Configure per-plugin settings ---
  plugin_arch::PluginManager::ConfigMap config = {
      {"Logger", {{"tag", "MANAGED"}}},
  };

  // --- Load all plugins with automatic dependency ordering ---
  std::cout << "=== Loading plugins from: " << plugin_dir << " ===\n\n";

  plugin_arch::PluginManager manager;
  try {
    manager.load_all(plugin_dir, config);
  } catch (const std::exception& e) {
    std::cerr << "Failed to load plugins: " << e.what() << "\n";
    return 1;
  }

  std::cout << "\nLoaded " << manager.instances().size() << " plugins\n\n";

  // --- Use the aggregator through the service locator ---
  auto aggregator = manager.locator().get<examples::IAggregator>("aggregator");
  if (!aggregator) {
    std::cerr << "Aggregator service not found\n";
    return 1;
  }

  std::cout << "=== Running aggregation ===\n\n";
  std::vector<std::string> items = {"hello", "plugin", "manager"};
  std::string result = aggregator->aggregate(items);
  std::cout << "\n" << result;

  // --- Shutdown in reverse order ---
  std::cout << "=== Shutting down ===\n\n";
  manager.shutdown();

  return 0;
}
