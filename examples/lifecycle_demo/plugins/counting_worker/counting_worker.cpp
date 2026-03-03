#include "counting_worker.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>

#include "PluginExport.hpp"

namespace examples {

std::string CountingWorker::process(const std::string& item) {
  ++counter_;
  std::string result = item;
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return std::toupper(c); });
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
