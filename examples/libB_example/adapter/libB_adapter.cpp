// Adapter implementation — every method just delegates to libB::Stats.

#include "libB_adapter.hpp"

#include "PluginFactory.hpp"

namespace examples {

LibBAdapter::LibBAdapter() : impl_(std::make_unique<libB::Stats>()) {}

double LibBAdapter::mean(const std::vector<double>& data) {
  return impl_->mean(data);
}

double LibBAdapter::stddev(const std::vector<double>& data) {
  return impl_->stddev(data);
}

double LibBAdapter::median(const std::vector<double>& data) {
  return impl_->median(data);
}

}  // namespace examples

REGISTER_PLUGIN(examples::LibBAdapter)
