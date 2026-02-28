// Service locator — lets plugins discover and call each other at runtime.
//
// The host registers loaded plugin instances as services. Plugins that need
// other plugins query the locator by type string.
//
// Lifetime rule: destroy the ServiceLocator (or call clear()) before
// destroying the PluginLoaders, because the shared_ptr custom deleters
// point into loaded libraries.

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "IPlugin.hpp"

namespace plugin_arch {

class ServiceLocator {
 public:
  // Register a loaded plugin as a service.
  void add(std::shared_ptr<IPlugin> service) {
    services_.push_back(std::move(service));
  }

  // Get the first service matching the given type string.
  // Returns nullptr if no match is found.
  template <typename T>
  std::shared_ptr<T> get(const std::string& type) const {
    for (const auto& svc : services_) {
      if (svc->type() == type) {
        auto casted = std::dynamic_pointer_cast<T>(svc);
        if (casted) return casted;
      }
    }
    return nullptr;
  }

  // Get all services matching the given type string.
  template <typename T>
  std::vector<std::shared_ptr<T>> get_all(const std::string& type) const {
    std::vector<std::shared_ptr<T>> result;
    for (const auto& svc : services_) {
      if (svc->type() == type) {
        auto casted = std::dynamic_pointer_cast<T>(svc);
        if (casted) result.push_back(std::move(casted));
      }
    }
    return result;
  }

  void clear() { services_.clear(); }

  std::size_t size() const { return services_.size(); }

 private:
  std::vector<std::shared_ptr<IPlugin>> services_;
};

// Opt-in mixin for plugins that need access to the service locator.
// The host detects this via dynamic_cast and injects the locator.
class IServiceAware {
 public:
  virtual ~IServiceAware() = default;
  virtual void set_service_locator(ServiceLocator& locator) = 0;
};

}  // namespace plugin_arch
