// Opt-in mixin for plugins that declare conflicts with other plugin types.
//
// Same pattern as IDependencyAware — detected via dynamic_cast.
// PluginManager refuses to load two plugins that conflict with each other.

#pragma once

#include <string>
#include <vector>

namespace plugin_arch {

class IConflictAware {
 public:
  virtual ~IConflictAware() = default;

  // Returns type strings of plugins this plugin conflicts with.
  // PluginManager will not load both simultaneously.
  virtual std::vector<std::string> conflicts() const = 0;
};

}  // namespace plugin_arch
