// Adapter plugin — wraps the pre-built libB into a plugin.
//
// You can't modify libB (no source code). So you write this thin wrapper that:
//   1. Implements IStatsEngine (the plugin interface)
//   2. Holds an instance of libB::Stats internally
//   3. Delegates every call to it
//
// This is the Adapter pattern applied to plugin loading.

#pragma once

#include <memory>

#include "IStatsEngine.hpp"
#include "libB.hpp"

namespace examples {

class LibBAdapter : public IStatsEngine {
 public:
  LibBAdapter();

  // Plugin metadata
  const std::string& name() const override { static const std::string s("LibBAdapter"); return s; }
  const std::string& version() const override { static const std::string s("1.0.0"); return s; }
  const std::string& type() const override { static const std::string s("stats_engine"); return s; }

  // Forwarded to libB::Stats
  double mean(const std::vector<double>& data) override;
  double stddev(const std::vector<double>& data) override;
  double median(const std::vector<double>& data) override;

 private:
  std::unique_ptr<libB::Stats> impl_;
};

}  // namespace examples
