// libB implementation — pretend you do NOT have this file.
// It exists only to produce the .dylib artifact.

#include "libB.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace libB {

Stats::Stats() = default;
Stats::~Stats() = default;

double Stats::mean(const std::vector<double>& data) const {
  if (data.empty()) return 0.0;
  double sum = std::accumulate(data.begin(), data.end(), 0.0);
  return sum / static_cast<double>(data.size());
}

double Stats::stddev(const std::vector<double>& data) const {
  if (data.size() < 2) return 0.0;
  double m = mean(data);
  double sq_sum = 0.0;
  for (double v : data) {
    sq_sum += (v - m) * (v - m);
  }
  return std::sqrt(sq_sum / static_cast<double>(data.size() - 1));
}

double Stats::median(std::vector<double> data) const {
  if (data.empty()) return 0.0;
  std::sort(data.begin(), data.end());
  size_t n = data.size();
  if (n % 2 == 0) {
    return (data[n / 2 - 1] + data[n / 2]) / 2.0;
  }
  return data[n / 2];
}

}  // namespace libB
