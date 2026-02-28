// Interface for a worker plugin that processes string items.

#pragma once

#include <string>

#include "IPlugin.hpp"

namespace examples {

class IWorker : public plugin_arch::IPlugin {
 public:
  virtual std::string process(const std::string& item) = 0;
};

}  // namespace examples
