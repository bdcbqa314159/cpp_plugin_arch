// Interface for a report generator plugin.
// Takes raw data and produces a formatted report string.

#pragma once

#include <string>
#include <vector>

#include "IPlugin.hpp"

namespace examples {

class IReportGenerator : public plugin_arch::IPlugin {
 public:
  virtual std::string generate(const std::vector<double>& data) = 0;
};

}  // namespace examples
