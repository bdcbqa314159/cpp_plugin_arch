// Registry demo — discovers ALL plugins in a directory using PluginRegistry
// instead of hardcoded filename conventions. Shows querying by type and name.

#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "PluginRegistry.hpp"

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
  fs::path plugin_dir = fs::path(argv[0]).parent_path();
  if (argc > 1) {
    plugin_dir = argv[1];
  }

  // --- Scan ---
  plugin_arch::PluginRegistry registry;
  auto found = registry.scan(plugin_dir);

  std::cout << "Scanned: " << plugin_dir << "\n";
  std::cout << "Found:   " << found << " plugin(s)\n";
  std::cout << "Skipped: " << registry.errors().size() << " library(s)\n\n";

  // --- List all discovered plugins ---
  std::cout << "=== All plugins ===\n";
  for (const auto& entry : registry.entries()) {
    std::cout << "  " << entry.name
              << " v" << entry.version
              << "  (type: " << entry.type << ")"
              << "  [" << entry.path.filename().string() << "]\n";
  }
  std::cout << "\n";

  // --- Group by type ---
  std::map<std::string, std::vector<const plugin_arch::PluginEntry*>> by_type;
  for (const auto& entry : registry.entries()) {
    by_type[entry.type].push_back(&entry);
  }

  std::cout << "=== Grouped by type ===\n";
  for (const auto& [type, plugins] : by_type) {
    std::cout << "  [" << type << "]\n";
    for (const auto* p : plugins) {
      std::cout << "    - " << p->name << " v" << p->version << "\n";
    }
  }
  std::cout << "\n";

  // --- Query by type ---
  auto calculators = registry.get_all("calculator");
  std::cout << "=== Query: type == \"calculator\" ===\n";
  if (calculators.empty()) {
    std::cout << "  (none found)\n";
  } else {
    for (const auto& c : calculators) {
      std::cout << "  " << c.name << " v" << c.version << "\n";
    }
  }
  std::cout << "\n";

  // --- Query by name ---
  std::cout << "=== Query: name == \"BasicCalc\" ===\n";
  if (auto entry = registry.get("BasicCalc")) {
    std::cout << "  Found: " << entry->name
              << " v" << entry->version
              << " at " << entry->path.filename().string() << "\n";
  } else {
    std::cout << "  (not found)\n";
  }
  std::cout << "\n";

  // --- Errors ---
  if (!registry.errors().empty()) {
    std::cout << "=== Skipped libraries ===\n";
    for (const auto& err : registry.errors()) {
      std::cout << "  " << err.path.filename().string()
                << ": " << err.reason << "\n";
    }
    std::cout << "\n";
  }

  return 0;
}
