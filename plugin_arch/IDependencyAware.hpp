// Opt-in mixin for plugins that declare dependencies on other services.
//
// Same pattern as IServiceAware / ILifecycleAware — detected via dynamic_cast.
// PluginManager uses this to topologically sort the load order.

#pragma once

#include <string>
#include <vector>

namespace plugin_arch {

class IDependencyAware {
 public:
  virtual ~IDependencyAware() = default;

  // Returns the type strings of services this plugin requires.
  // PluginManager ensures these are loaded and registered before this plugin.
  virtual std::vector<std::string> dependencies() const = 0;
};

}  // namespace plugin_arch
