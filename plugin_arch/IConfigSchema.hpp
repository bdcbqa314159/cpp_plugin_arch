// Opt-in mixin for plugins that declare their configuration schema.
//
// Same pattern as IConfigurable — detected via dynamic_cast.
// PluginManager validates the config map against the schema before
// calling configure(). Missing required keys or unknown keys can be
// caught early with clear error messages.

#pragma once

#include <string>
#include <vector>

namespace plugin_arch {

struct ConfigKeyDef {
  std::string key;
  bool required = false;
  std::string default_value;   // used when key is absent and not required
  std::string description;     // human-readable, for documentation/tooling
};

class IConfigSchema {
 public:
  virtual ~IConfigSchema() = default;

  // Returns the schema: list of accepted config keys with metadata.
  virtual std::vector<ConfigKeyDef> config_schema() const = 0;
};

}  // namespace plugin_arch
