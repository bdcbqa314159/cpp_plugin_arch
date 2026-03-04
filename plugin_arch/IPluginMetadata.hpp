// Opt-in mixin for plugins that expose extended metadata beyond
// IPlugin's name/version/type.
//
// Same pattern as other opt-in mixins — detected via dynamic_cast.
// PluginRegistry indexes the extra fields when present.

#pragma once

#include <string>
#include <vector>

namespace plugin_arch {

class IPluginMetadata {
 public:
  virtual ~IPluginMetadata() = default;

  virtual const std::string& author() const = 0;
  virtual const std::string& description() const = 0;
  virtual const std::string& license() const = 0;
  virtual std::vector<std::string> capabilities() const { return {}; }
};

}  // namespace plugin_arch
