// Host application — loads libA as a plugin and uses it through the interface.
// This file never includes libA.hpp. It only knows ITextFormatter.

#include <filesystem>
#include <iostream>

#include "ITextFormatter.hpp"
#include "PluginLoader.hpp"
#include "platform/shared_lib.hpp"

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
  fs::path plugin_dir = fs::path(argv[0]).parent_path();
  if (plugin_dir.empty()) plugin_dir = fs::current_path();
  if (argc > 1) {
    plugin_dir = argv[1];
  }

  // Find the libA plugin
  auto ext = plugin_arch::platform::shared_lib_extension();
  fs::path libA_path;
  for (const auto& entry : fs::directory_iterator(plugin_dir)) {
    if (entry.path().extension() == ext &&
        entry.path().filename().string().find("libA") != std::string::npos) {
      libA_path = entry.path();
      break;
    }
  }

  if (libA_path.empty()) {
    std::cerr << "libA plugin not found in: " << plugin_dir << "\n";
    return 1;
  }

  // Load the plugin — this is all the host needs to do
  plugin_arch::PluginLoader<examples::ITextFormatter> loader(libA_path.string());
  auto formatter = loader.get_instance();

  // Query plugin metadata
  std::cout << "Loaded: " << formatter->name()
            << " v" << formatter->version()
            << " (type: " << formatter->type() << ")\n";
  std::cout << "From:   " << libA_path.filename() << "\n\n";

  // Use the plugin through the interface — no knowledge of LibA's internals
  std::string sample = "Hello Plugin World";
  std::cout << "  original:  " << sample << "\n";
  std::cout << "  to_upper:  " << formatter->to_upper(sample) << "\n";
  std::cout << "  to_lower:  " << formatter->to_lower(sample) << "\n";
  std::cout << "  reverse:   " << formatter->reverse(sample) << "\n";

  return 0;
}
