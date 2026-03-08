// Opt-in mixin for plugins that support health checking.
//
// Same pattern as ILifecycleAware — detected via dynamic_cast.
// PluginManager::check_health() polls all plugins that implement this mixin.
// Plugins that don't implement IHealthAware are implicitly considered healthy.

#pragma once

#include <string>

namespace plugin_arch {

struct HealthStatus {
  bool healthy = true;
  std::string message;  // empty = fine, non-empty = explanation
};

class IHealthAware {
 public:
  virtual ~IHealthAware() = default;

  // Fast-path boolean check.
  virtual bool is_healthy() const = 0;

  // Richer status with diagnostic message.
  virtual HealthStatus health_status() const = 0;
};

}  // namespace plugin_arch
