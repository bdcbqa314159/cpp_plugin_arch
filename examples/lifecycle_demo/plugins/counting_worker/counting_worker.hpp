// CountingWorker — a plugin that uppercases items and counts how many
// it has processed. Implements ILifecycleAware to demonstrate lifecycle hooks.

#pragma once

#include "ILifecycleAware.hpp"
#include "IWorker.hpp"

namespace examples {

class CountingWorker : public IWorker, public plugin_arch::ILifecycleAware {
 public:
  // IPlugin metadata
  std::string name() const override { return "CountingWorker"; }
  std::string version() const override { return "1.0.0"; }
  std::string type() const override { return "worker"; }

  // IWorker
  std::string process(const std::string& item) override;

  // ILifecycleAware
  void on_init() override;
  void on_shutdown() override;

 private:
  int counter_ = 0;
};

}  // namespace examples
