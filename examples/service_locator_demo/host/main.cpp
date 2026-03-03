// Service locator demo — plugin-to-plugin communication.
//
// The host loads three plugins, registers two of them as services in a
// ServiceLocator, then injects the locator into the third. The report
// generator plugin discovers and calls the other two through the locator.
//
// Host flow:
//   1. Load LibBAdapter (stats_engine) and LibA (text_formatter)
//   2. Register both in a ServiceLocator
//   3. Load ReportGenerator
//   4. Detect IServiceAware via dynamic_cast, inject the locator
//   5. Call report->generate(data)

#include <filesystem>
#include <iostream>
#include <vector>

#include "IReportGenerator.hpp"
#include "PluginLoader.hpp"
#include "ServiceLocator.hpp"
#include "platform/shared_lib.hpp"

// Only need the base types for registering in the locator
#include "IPlugin.hpp"

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
  fs::path plugin_dir = fs::path(argv[0]).parent_path();
  if (argc > 1) {
    plugin_dir = argv[1];
  }

  // --- Step 1: Find plugins ---
  auto stats_path = plugin_arch::platform::find_plugin(plugin_dir, "libB_adapter");
  auto formatter_path = plugin_arch::platform::find_plugin(plugin_dir, "libA");
  auto report_path = plugin_arch::platform::find_plugin(plugin_dir, "report_generator");

  if (stats_path.empty() || formatter_path.empty() || report_path.empty()) {
    std::cerr << "Missing plugins in: " << plugin_dir << "\n";
    if (stats_path.empty()) std::cerr << "  - libB_adapter not found\n";
    if (formatter_path.empty()) std::cerr << "  - libA not found\n";
    if (report_path.empty()) std::cerr << "  - report_generator not found\n";
    return 1;
  }

  // --- Step 2: Load service plugins ---
  // Use IPlugin as the loader type since we only need base interface for the locator
  plugin_arch::PluginLoader<plugin_arch::IPlugin> stats_loader(stats_path.string());
  auto stats = stats_loader.get_instance();
  std::cout << "Loaded: " << stats->name() << " v" << stats->version()
            << " (type: " << stats->type() << ")\n";

  plugin_arch::PluginLoader<plugin_arch::IPlugin> formatter_loader(formatter_path.string());
  auto formatter = formatter_loader.get_instance();
  std::cout << "Loaded: " << formatter->name() << " v" << formatter->version()
            << " (type: " << formatter->type() << ")\n";

  // --- Step 3: Register services in the locator ---
  plugin_arch::ServiceLocator locator;
  locator.add(stats);
  locator.add(formatter);
  std::cout << "\nServiceLocator: " << locator.size() << " services registered\n";

  // --- Step 4: Load the report generator and inject the locator ---
  plugin_arch::PluginLoader<examples::IReportGenerator> report_loader(report_path.string());
  auto report = report_loader.get_instance();
  std::cout << "Loaded: " << report->name() << " v" << report->version()
            << " (type: " << report->type() << ")\n";

  // Detect IServiceAware and inject
  auto* aware = dynamic_cast<plugin_arch::IServiceAware*>(report.get());
  if (aware) {
    aware->set_service_locator(locator);
    std::cout << "Injected service locator into " << report->name() << "\n\n";
  } else {
    std::cerr << report->name() << " does not implement IServiceAware\n";
    return 1;
  }

  // --- Step 5: Generate the report ---
  std::vector<double> data = {10.5, 20.3, 15.7, 8.2, 25.1, 12.8, 18.4};
  std::string result = report->generate(data);
  std::cout << result << "\n";

  return 0;
}
