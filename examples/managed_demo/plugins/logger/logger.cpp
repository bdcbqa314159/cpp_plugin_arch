#include "logger.hpp"

#include <iostream>

#include "PluginExport.hpp"

namespace examples {

void Logger::configure(const plugin_arch::PluginConfig& config) {
  if (auto it = config.find("tag"); it != config.end()) {
    tag_ = it->second;
  }
}

void Logger::set_event_bus(plugin_arch::EventBus& bus) {
  bus_ = &bus;
  // Subscribe to "log" topic — any plugin can publish log messages
  sub_id_ = bus_->subscribe("log",
      [this](const std::string& /*topic*/, const std::string& payload) {
        log(payload);
      });
}

void Logger::on_init() {
  std::cout << "  [Logger] initialized with tag: " << tag_ << "\n";
}

void Logger::on_shutdown() {
  if (bus_ && sub_id_ != plugin_arch::EventBus::invalid_id) {
    bus_->unsubscribe(sub_id_);
    sub_id_ = plugin_arch::EventBus::invalid_id;
  }
  bus_ = nullptr;
  std::cout << "  [Logger] shutting down\n";
}

void Logger::log(const std::string& message) {
  std::cout << "  [" << tag_ << "] " << message << "\n";
}

}  // namespace examples

REGISTER_PLUGIN(examples::Logger)
