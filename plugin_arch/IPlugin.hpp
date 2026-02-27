// Base plugin interface. Every plugin inherits this so the host can query
// name, version, and type before casting to a domain-specific interface.

#pragma once

#include <string>

namespace plugin_arch {

class IPlugin {
 public:
  virtual ~IPlugin() = default;

  virtual std::string name() const = 0;
  virtual std::string version() const = 0;
  virtual std::string type() const = 0;
};

}  // namespace plugin_arch
