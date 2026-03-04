// Processor plugin — depends on the "logger" service.
// Implements IDependencyAware to declare the dependency.

#pragma once

#include <string>
#include <vector>

#include "IDependencyAware.hpp"
#include "ILifecycleAware.hpp"
#include "IProcessor.hpp"
#include "IServiceAware.hpp"
#include "ServiceLocator.hpp"

namespace examples {

class Processor : public IProcessor,
                  public plugin_arch::IServiceAware,
                  public plugin_arch::ILifecycleAware,
                  public plugin_arch::IDependencyAware {
 public:
  const std::string& name() const override { static const std::string s("Processor"); return s; }
  const std::string& version() const override { static const std::string s("1.0.0"); return s; }
  const std::string& type() const override { static const std::string s("processor"); return s; }

  std::vector<std::string> dependencies() const override;
  void set_service_locator(plugin_arch::ServiceLocator& locator) override;
  void on_init() override;
  void on_shutdown() override;
  std::vector<std::string> process(const std::vector<std::string>& items) override;

 private:
  plugin_arch::ServiceLocator* locator_ = nullptr;
};

}  // namespace examples
