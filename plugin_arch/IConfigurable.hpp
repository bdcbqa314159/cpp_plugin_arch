// Opt-in configuration mixin for plugins that accept key-value parameters.
//
// Same pattern as IServiceAware / ILifecycleAware — detected via dynamic_cast.
// Host call order: construct → configure() → set_service_locator() → on_init() → use → on_shutdown() → destroy.

#pragma once

#include <string>
#include <unordered_map>

namespace plugin_arch {

using PluginConfig = std::unordered_map<std::string, std::string>;

class IConfigurable {
 public:
  virtual ~IConfigurable() = default;
  virtual void configure(const PluginConfig& config) = 0;
};

}  // namespace plugin_arch
