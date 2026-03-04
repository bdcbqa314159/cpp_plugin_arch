// Simple logger plugin — no dependencies, provides a "logger" service.

#pragma once

#include "IConfigurable.hpp"
#include "ILifecycleAware.hpp"
#include "ILogger.hpp"

namespace examples {

class Logger : public ILogger,
               public plugin_arch::ILifecycleAware,
               public plugin_arch::IConfigurable {
 public:
  const std::string& name() const override { static const std::string s("Logger"); return s; }
  const std::string& version() const override { static const std::string s("1.0.0"); return s; }
  const std::string& type() const override { static const std::string s("logger"); return s; }

  void configure(const plugin_arch::PluginConfig& config) override;
  void on_init() override;
  void on_shutdown() override;
  void log(const std::string& message) override;

 private:
  std::string tag_ = "LOG";
};

}  // namespace examples
