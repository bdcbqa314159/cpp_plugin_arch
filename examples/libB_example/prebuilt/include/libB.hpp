// libB — a third-party stats library.
// This header is all you have. No source code. No plugin awareness.
// Just a regular class exported from a shared library.

#pragma once

#include <vector>

namespace libB {

class Stats {
 public:
  Stats();
  ~Stats();

  double mean(const std::vector<double>& data) const;
  double stddev(const std::vector<double>& data) const;
  double median(std::vector<double> data) const;
};

}  // namespace libB
