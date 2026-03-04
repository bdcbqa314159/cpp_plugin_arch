// CountingWorker — a plugin that uppercases items and counts how many
// it has processed. Implements ILifecycleAware to demonstrate lifecycle hooks.

#pragma once

#include "IConfigurable.hpp"
#include "ILifecycleAware.hpp"
#include "IWorker.hpp"

namespace examples {

class CountingWorker : public IWorker,
                       public plugin_arch::ILifecycleAware,
                       public plugin_arch::IConfigurable {
 public:
  // IPlugin metadata
  const std::string& name() const override { static const std::string s("CountingWorker"); return s; }
  const std::string& version() const override { static const std::string s("1.0.0"); return s; }
  const std::string& type() const override { static const std::string s("worker"); return s; }

  // IConfigurable
  void configure(const plugin_arch::PluginConfig& config) override;

  // IWorker
  std::string process(const std::string& item) override;

  // ILifecycleAware
  void on_init() override;
  void on_shutdown() override;

 private:
  int counter_ = 0;
  std::string prefix_;
};

}  // namespace examples
