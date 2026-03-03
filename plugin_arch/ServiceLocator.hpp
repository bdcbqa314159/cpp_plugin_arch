// Service locator — lets plugins discover and call each other at runtime.
//
// The host registers loaded plugin instances as services. Plugins that need
// other plugins query the locator by type string.
//
// Stores weak_ptr — the ServiceLocator does not own the plugins. The host's
// shared_ptrs are the owners, so destruction order no longer matters.

#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "IPlugin.hpp"

namespace plugin_arch {

class ServiceLocator {
 public:
  // Register a loaded plugin as a service.
  // The locator holds a weak reference — it does not extend the plugin's lifetime.
  void add(std::shared_ptr<IPlugin> service) {
    if (!service) return;
    services_.push_back(service);
  }

  // Get the first service matching the given type string.
  // Returns nullptr if no match is found (or if the plugin has been destroyed).
  template <typename T>
  std::shared_ptr<T> get(const std::string& type) const {
    for (const auto& weak_svc : services_) {
      auto svc = weak_svc.lock();
      if (!svc) continue;
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
    for (const auto& weak_svc : services_) {
      auto svc = weak_svc.lock();
      if (!svc) continue;
      if (svc->type() == type) {
        auto casted = std::dynamic_pointer_cast<T>(svc);
        if (casted) result.push_back(std::move(casted));
      }
    }
    return result;
  }

  // Remove expired entries (plugins that have been destroyed).
  void cleanup() {
    services_.erase(
        std::remove_if(services_.begin(), services_.end(),
                       [](const std::weak_ptr<IPlugin>& w) {
                         return w.expired();
                       }),
        services_.end());
  }

  void clear() { services_.clear(); }

  std::size_t size() const { return services_.size(); }

 private:
  std::vector<std::weak_ptr<IPlugin>> services_;
};

// Opt-in mixin for plugins that need access to the service locator.
// The host detects this via dynamic_cast and injects the locator.
class IServiceAware {
 public:
  virtual ~IServiceAware() = default;
  virtual void set_service_locator(ServiceLocator& locator) = 0;
};

}  // namespace plugin_arch
