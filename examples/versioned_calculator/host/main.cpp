// Versioned calculator demo — interface versioning via inheritance.
//
// Demonstrates how v1 plugins (BasicCalc, ScientificCalc) coexist with
// a v2 plugin (TrigCalc) in the same host.  The host loads all plugins
// as ICalculator (v1), exercises the common methods, then uses
// dynamic_cast<ICalculatorV2*> to detect extended capabilities.
//
// No v1 code is modified.  ICalculator is frozen.

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "ICalculatorV2.hpp"
#include "PluginLoader.hpp"
#include "platform/shared_lib.hpp"

namespace fs = std::filesystem;

static void exercise_v1(examples::ICalculator& calc) {
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

static void exercise_v2(examples::ICalculatorV2& calc) {
  const double pi = 3.14159265358979323846;
  std::cout << "  sin(pi/6)       = " << calc.sin(pi / 6) << "\n";
  std::cout << "  cos(pi/3)       = " << calc.cos(pi / 3) << "\n";
  std::cout << "  log(2.71828)    = " << calc.log(2.71828) << "\n";
}

int main(int argc, char* argv[]) {
  fs::path plugin_dir = fs::path(argv[0]).parent_path();
  if (plugin_dir.empty()) plugin_dir = fs::current_path();
  if (argc > 1) {
    plugin_dir = argv[1];
  }

  auto ext = plugin_arch::platform::shared_lib_extension();

  // Discover *calc* plugins in the directory
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

  std::sort(plugin_paths.begin(), plugin_paths.end());

  std::cout << "Found " << plugin_paths.size() << " calculator plugin(s) in "
            << plugin_dir << "\n\n";

  for (const auto& path : plugin_paths) {
    try {
      // Load every plugin as ICalculator (v1) — always works
      plugin_arch::PluginLoader<examples::ICalculator> loader(path.string());
      auto calc = loader.get_instance();

      std::cout << "[" << calc->name() << " v" << calc->version()
                << "]  (type: " << calc->type() << ")\n";
      std::cout << "  loaded from: " << path.filename() << "\n";

      // Exercise v1 methods
      exercise_v1(*calc);

      // Probe for v2 via dynamic_cast
      auto* v2 = dynamic_cast<examples::ICalculatorV2*>(calc.get());
      if (v2) {
        std::cout << "  --- v2 methods (ICalculatorV2) ---\n";
        exercise_v2(*v2);
      } else {
        std::cout << "  [v2 not supported]\n";
      }

      std::cout << "\n";
    } catch (const std::exception& e) {
      std::cerr << "Failed to load " << path.filename() << ": " << e.what()
                << "\n\n";
    }
  }

  return 0;
}
