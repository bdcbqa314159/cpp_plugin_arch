#include "processor.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>

#include "ILogger.hpp"
#include "PluginExport.hpp"

namespace examples {

std::vector<std::string> Processor::dependencies() const {
  return {"logger"};
}

void Processor::set_service_locator(plugin_arch::ServiceLocator& locator) {
  locator_ = &locator;
}

void Processor::on_init() {
  std::cout << "  [Processor] initialized\n";
}

void Processor::on_shutdown() {
  std::cout << "  [Processor] shutting down\n";
}

std::vector<std::string> Processor::process(const std::vector<std::string>& items) {
  auto logger = locator_->get<ILogger>("logger");

  std::vector<std::string> results;
  results.reserve(items.size());
  for (const auto& item : items) {
    std::string upper = item;
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    results.push_back(upper);
    if (logger) {
      logger->log("processed: " + item + " -> " + upper);
    }
  }
  return results;
}

}  // namespace examples

REGISTER_PLUGIN(examples::Processor)
