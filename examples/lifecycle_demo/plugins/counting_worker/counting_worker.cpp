#include "counting_worker.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>

#include "PluginExport.hpp"

namespace examples {

void CountingWorker::configure(const plugin_arch::PluginConfig& config) {
  if (auto it = config.find("prefix"); it != config.end()) {
    prefix_ = it->second;
    std::cout << "  [CountingWorker] configured prefix: \"" << prefix_ << "\"\n";
  }
}

std::string CountingWorker::process(const std::string& item) {
  ++counter_;
  std::string result = item;
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return std::toupper(c); });
  if (!prefix_.empty()) {
    result = prefix_ + result;
  }
  return result;
}

void CountingWorker::on_init() {
  counter_ = 0;
  std::cout << "  [CountingWorker] initialized (counter reset)\n";
}

void CountingWorker::on_shutdown() {
  std::cout << "  [CountingWorker] shutting down (processed " << counter_
            << " items)\n";
}

}  // namespace examples

REGISTER_PLUGIN(examples::CountingWorker)
