// Plugin interface for statistics engines.
// This is YOUR interface — you define it based on what you need from libB.

#pragma once

#include <string>
#include <vector>

#include "IPlugin.hpp"

namespace examples {

class IStatsEngine : public plugin_arch::IPlugin {
 public:
  virtual double mean(const std::vector<double>& data) = 0;
  virtual double stddev(const std::vector<double>& data) = 0;
  virtual double median(const std::vector<double>& data) = 0;
};

}  // namespace examples
