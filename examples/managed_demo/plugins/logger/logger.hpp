// Simple logger plugin — no dependencies, provides a "logger" service.

#pragma once

#include "EventBus.hpp"
#include "IConfigurable.hpp"
#include "IEventAware.hpp"
#include "ILifecycleAware.hpp"
#include "ILogger.hpp"

namespace examples {

class Logger : public ILogger,
               public plugin_arch::ILifecycleAware,
               public plugin_arch::IConfigurable,
               public plugin_arch::IEventAware {
 public:
  const std::string& name() const override { static const std::string s("Logger"); return s; }
  const std::string& version() const override { static const std::string s("1.0.0"); return s; }
  const std::string& type() const override { static const std::string s("logger"); return s; }

  void configure(const plugin_arch::PluginConfig& config) override;
  void set_event_bus(plugin_arch::EventBus& bus) override;
  void on_init() override;
  void on_shutdown() override;
  void log(const std::string& message) override;

 private:
  std::string tag_ = "LOG";
  plugin_arch::EventBus* bus_ = nullptr;
  plugin_arch::EventBus::SubscriptionId sub_id_ = plugin_arch::EventBus::invalid_id;
};

}  // namespace examples
