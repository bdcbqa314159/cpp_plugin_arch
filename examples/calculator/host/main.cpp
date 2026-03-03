#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "ICalculator.hpp"
#include "PluginLoader.hpp"
#include "platform/shared_lib.hpp"

namespace fs = std::filesystem;

static void exercise(examples::ICalculator& calc) {
  std::cout << "  add(10, 3)      = " << calc.add(10, 3) << "\n";
  std::cout << "  subtract(10, 3) = " << calc.subtract(10, 3) << "\n";
  std::cout << "  multiply(10, 3) = " << calc.multiply(10, 3) << "\n";
  std::cout << "  divide(10, 3)   = " << calc.divide(10, 3) << "\n";

  try {
    double pw = calc.power(2, 8);
    std::cout << "  power(2, 8)     = " << pw << "\n";
  } catch (const std::domain_error&) {
    std::cout << "  power(2, 8)     = [not supported]\n";
  }

  try {
    double sq = calc.sqrt(144);
    std::cout << "  sqrt(144)       = " << sq << "\n";
  } catch (const std::domain_error&) {
    std::cout << "  sqrt(144)       = [not supported]\n";
  }
}

int main(int argc, char* argv[]) {
  // Default: look for plugins next to the host binary
  fs::path plugin_dir = fs::path(argv[0]).parent_path();
  if (argc > 1) {
    plugin_dir = argv[1];
  }

  auto ext = plugin_arch::platform::shared_lib_extension();

  // Discover plugin files in the directory
  std::vector<fs::path> plugin_paths;
  for (const auto& entry : fs::directory_iterator(plugin_dir)) {
    if (entry.path().extension() == ext &&
        entry.path().filename().string().find("calc") != std::string::npos) {
      plugin_paths.push_back(entry.path());
    }
  }

  if (plugin_paths.empty()) {
    std::cerr << "No calculator plugins found in: " << plugin_dir << "\n";
    return 1;
  }

  std::cout << "Found " << plugin_paths.size() << " plugin(s) in "
            << plugin_dir << "\n\n";

  for (const auto& path : plugin_paths) {
    try {
      plugin_arch::PluginLoader<examples::ICalculator> loader(path.string());
      auto calc = loader.get_instance();

      std::cout << "[" << calc->name() << " v" << calc->version()
                << "]  (type: " << calc->type() << ")\n";
      std::cout << "  loaded from: " << path.filename() << "\n";

      exercise(*calc);
      std::cout << "\n";
    } catch (const std::exception& e) {
      std::cerr << "Failed to load " << path.filename() << ": " << e.what()
                << "\n\n";
    }
  }

  return 0;
}
