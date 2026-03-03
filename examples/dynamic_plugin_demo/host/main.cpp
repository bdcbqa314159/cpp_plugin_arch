// Host that loads a plugin with no interface header.
// Only framework headers are needed — no IPlugin, no ICalculator, nothing
// from the plugin itself.

#include "DynamicLibrary.hpp"
#include "PluginDescriptor.hpp"
#include "platform/shared_lib.hpp"

#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
  namespace fs = std::filesystem;

  // Plugin lives next to the host binary (both in CMAKE_BINARY_DIR/bin).
  auto lib_name = "libmath_functions" +
                  std::string(plugin_arch::platform::shared_lib_extension());
  auto plugin_dir = fs::path(argv[0]).parent_path();
  if (argc > 1) {
    plugin_dir = argv[1];
  }
  auto path = (plugin_dir / lib_name).string();

  std::cout << "Loading: " << lib_name << "\n";

  try {
    plugin_arch::DynamicLibrary lib(path);

    // --- Step 1: Discover the API via plugin_describe() ---
    using DescribeFunc = const plugin_arch::PluginDescriptor* (*)();
    auto describe = lib.resolve<DescribeFunc>("plugin_describe");
    auto* desc = describe();

    std::cout << "\nPlugin: " << desc->name << " v" << desc->version << "\n";
    std::cout << "Exported functions (" << desc->function_count << "):\n";
    for (int i = 0; i < desc->function_count; ++i) {
      auto name = std::string(desc->functions[i].name);
      auto pad_len = name.size() < 14 ? 14 - name.size() : 1;
      auto pad = std::string(pad_len, ' ');
      std::cout << "  " << name << pad << ": "
                << desc->functions[i].signature << "\n";
    }

    // --- Step 2: Resolve and call functions by name ---
    using BinaryFunc = double (*)(double, double);
    using UnaryFunc = double (*)(double);

    auto add = lib.resolve<BinaryFunc>("math_add");
    auto multiply = lib.resolve<BinaryFunc>("math_multiply");
    auto sqrt_fn = lib.resolve<UnaryFunc>("math_sqrt");

    std::cout << "\nCalling functions:\n";
    std::cout << "  math_add(10, 3)      = " << add(10, 3) << "\n";
    std::cout << "  math_multiply(10, 3) = " << multiply(10, 3) << "\n";
    std::cout << "  math_sqrt(144)       = " << sqrt_fn(144) << "\n";

    // --- Step 3: Probe for optional functions ---
    std::cout << "\nProbing for optional functions:\n";
    std::cout << "  math_divide: "
              << (lib.has("math_divide") ? "available" : "not available")
              << "\n";

    // --- Step 4: Check for legacy convention symbols ---
    std::cout << "\nLegacy convention check:\n";
    std::cout << "  has 'allocator':       "
              << (lib.has("allocator") ? "yes" : "no") << "\n";
    std::cout << "  has 'deallocator':     "
              << (lib.has("deallocator") ? "yes" : "no") << "\n";
    std::cout << "  has 'plugin_describe': "
              << (lib.has("plugin_describe") ? "yes" : "no") << "\n";

  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
