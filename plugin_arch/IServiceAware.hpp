// Opt-in mixin for plugins that need access to the service locator.
// The host detects this via dynamic_cast and injects the locator.

#pragma once

namespace plugin_arch {

class ServiceLocator;

class IServiceAware {
 public:
  virtual ~IServiceAware() = default;
  virtual void set_service_locator(ServiceLocator& locator) = 0;
};

}  // namespace plugin_arch
