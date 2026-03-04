// Interface for a logger service plugin.

#pragma once

#include <string>

#include "IPlugin.hpp"

namespace examples {

class ILogger : public plugin_arch::IPlugin {
 public:
  virtual void log(const std::string& message) = 0;
};

}  // namespace examples
