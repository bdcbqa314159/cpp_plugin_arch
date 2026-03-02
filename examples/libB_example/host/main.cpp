// Host — loads the adapter plugin and uses libB through the plugin interface.
// This file knows nothing about libB. It only knows IStatsEngine.

#include <filesystem>
#include <iostream>
#include <vector>

#include "IStatsEngine.hpp"
#include "PluginLoader.hpp"
#include "platform/shared_lib.hpp"

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
  fs::path plugin_dir = fs::path(argv[0]).parent_path();
  if (argc > 1) {
    plugin_dir = argv[1];
  }

  auto ext = plugin_arch::platform::shared_lib_extension();
  fs::path adapter_path;
  for (const auto& entry : fs::directory_iterator(plugin_dir)) {
    if (entry.path().extension() == ext &&
        entry.path().filename().string().find("adapter") != std::string::npos) {
      adapter_path = entry.path();
      break;
    }
  }

  if (adapter_path.empty()) {
    std::cerr << "No adapter plugin found in: " << plugin_dir << "\n";
    return 1;
  }

  plugin_arch::PluginLoader<examples::IStatsEngine> loader(adapter_path.string());
  auto stats = loader.get_instance();

  std::cout << "Loaded: " << stats->name()
            << " v" << stats->version()
            << " (type: " << stats->type() << ")\n";
  std::cout << "From:   " << adapter_path.filename() << "\n\n";

  // Use the plugin — we're calling libB under the hood, but the host doesn't know that
  std::vector<double> data = {2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0};

  std::cout << "  data:   {2, 4, 4, 4, 5, 5, 7, 9}\n";
  std::cout << "  mean:   " << stats->mean(data) << "\n";
  std::cout << "  stddev: " << stats->stddev(data) << "\n";
  std::cout << "  median: " << stats->median(data) << "\n";

  return 0;
}
