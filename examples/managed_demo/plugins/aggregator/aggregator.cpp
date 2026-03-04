#include "aggregator.hpp"

#include <iostream>
#include <sstream>

#include "ILogger.hpp"
#include "IProcessor.hpp"
#include "PluginExport.hpp"

namespace examples {

std::vector<std::string> Aggregator::dependencies() const {
  return {"logger", "processor"};
}

void Aggregator::set_service_locator(plugin_arch::ServiceLocator& locator) {
  locator_ = &locator;
}

void Aggregator::on_init() {
  std::cout << "  [Aggregator] initialized\n";
}

void Aggregator::on_shutdown() {
  std::cout << "  [Aggregator] shutting down\n";
}

std::string Aggregator::aggregate(const std::vector<std::string>& items) {
  auto logger = locator_->get<ILogger>("logger");
  auto processor = locator_->get<IProcessor>("processor");

  if (logger) {
    logger->log("aggregating " + std::to_string(items.size()) + " items");
  }

  // Process items through the processor service
  std::vector<std::string> processed;
  if (processor) {
    processed = processor->process(items);
  } else {
    processed = items;
  }

  // Aggregate all processed items into a single string
  std::ostringstream result;
  result << "Aggregated " << processed.size() << " items:\n";
  for (std::size_t i = 0; i < processed.size(); ++i) {
    result << "  [" << i << "] " << processed[i] << "\n";
  }

  if (logger) {
    logger->log("aggregation complete");
  }

  return result.str();
}

}  // namespace examples

REGISTER_PLUGIN(examples::Aggregator)
