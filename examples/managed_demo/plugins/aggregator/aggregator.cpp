#include "aggregator.hpp"

#include <iostream>
#include <sstream>

#include "IProcessor.hpp"
#include "PluginExport.hpp"

namespace examples {

std::vector<std::string> Aggregator::dependencies() const {
  return {"logger", "processor"};
}

void Aggregator::set_service_locator(plugin_arch::ServiceLocator& locator) {
  locator_ = &locator;
}

void Aggregator::set_event_bus(plugin_arch::EventBus& bus) {
  bus_ = &bus;
}

void Aggregator::on_init() {
  std::cout << "  [Aggregator] initialized\n";
}

void Aggregator::on_shutdown() {
  std::cout << "  [Aggregator] shutting down\n";
}

std::string Aggregator::aggregate(const std::vector<std::string>& items) {
  auto processor = locator_->get<IProcessor>("processor");

  // Publish via event bus instead of calling logger directly
  if (bus_) {
    bus_->publish("log", "aggregating " + std::to_string(items.size()) + " items");
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

  if (bus_) {
    bus_->publish("log", "aggregation complete");
  }

  return result.str();
}

}  // namespace examples

REGISTER_PLUGIN(examples::Aggregator)
