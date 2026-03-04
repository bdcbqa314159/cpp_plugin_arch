// Interface for a data processor plugin.

#pragma once

#include <string>
#include <vector>

#include "IPlugin.hpp"

namespace examples {

class IProcessor : public plugin_arch::IPlugin {
 public:
  virtual std::vector<std::string> process(const std::vector<std::string>& items) = 0;
};

}  // namespace examples
