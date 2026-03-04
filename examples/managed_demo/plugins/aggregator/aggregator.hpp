// Aggregator plugin — depends on both "logger" and "processor" services.
// Implements IDependencyAware to declare both dependencies.

#pragma once

#include <string>
#include <vector>

#include "IAggregator.hpp"
#include "IDependencyAware.hpp"
#include "ILifecycleAware.hpp"
#include "IServiceAware.hpp"
#include "ServiceLocator.hpp"

namespace examples {

class Aggregator : public IAggregator,
                   public plugin_arch::IServiceAware,
                   public plugin_arch::ILifecycleAware,
                   public plugin_arch::IDependencyAware {
 public:
  const std::string& name() const override { static const std::string s("Aggregator"); return s; }
  const std::string& version() const override { static const std::string s("1.0.0"); return s; }
  const std::string& type() const override { static const std::string s("aggregator"); return s; }

  std::vector<std::string> dependencies() const override;
  void set_service_locator(plugin_arch::ServiceLocator& locator) override;
  void on_init() override;
  void on_shutdown() override;
  std::string aggregate(const std::vector<std::string>& items) override;

 private:
  plugin_arch::ServiceLocator* locator_ = nullptr;
};

}  // namespace examples
