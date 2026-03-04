#include "logger.hpp"

#include <iostream>

#include "PluginExport.hpp"

namespace examples {

void Logger::configure(const plugin_arch::PluginConfig& config) {
  if (auto it = config.find("tag"); it != config.end()) {
    tag_ = it->second;
  }
}

void Logger::on_init() {
  std::cout << "  [Logger] initialized with tag: " << tag_ << "\n";
}

void Logger::on_shutdown() {
  std::cout << "  [Logger] shutting down\n";
}

void Logger::log(const std::string& message) {
  std::cout << "  [" << tag_ << "] " << message << "\n";
}

}  // namespace examples

REGISTER_PLUGIN(examples::Logger)
