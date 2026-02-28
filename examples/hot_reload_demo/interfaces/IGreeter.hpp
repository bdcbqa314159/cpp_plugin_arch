// Simple plugin interface for the hot-reload demo.

#pragma once

#include "IPlugin.hpp"

#include <string>

namespace examples {

class IGreeter : public plugin_arch::IPlugin {
 public:
  virtual std::string greet() = 0;
};

}  // namespace examples
