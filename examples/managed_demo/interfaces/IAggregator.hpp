// Interface for an aggregator plugin that depends on logger and processor.

#pragma once

#include <string>
#include <vector>

#include "IPlugin.hpp"

namespace examples {

class IAggregator : public plugin_arch::IPlugin {
 public:
  virtual std::string aggregate(const std::vector<std::string>& items) = 0;
};

}  // namespace examples
