// Report generator — discovers stats_engine and text_formatter through the
// service locator, computes stats, formats the output, returns the report.

#include "report_generator.hpp"

#include <sstream>
#include <stdexcept>

#include "IStatsEngine.hpp"
#include "ITextFormatter.hpp"
#include "PluginExport.hpp"

namespace examples {

void ReportGenerator::set_service_locator(plugin_arch::ServiceLocator& locator) {
  locator_ = &locator;
}

std::string ReportGenerator::generate(const std::vector<double>& data) {
  if (!locator_) {
    throw std::runtime_error("ReportGenerator: no service locator injected");
  }

  // Discover the stats engine through the locator
  auto stats = locator_->get<IStatsEngine>("stats_engine");
  if (!stats) {
    throw std::runtime_error("ReportGenerator: stats_engine service not found");
  }

  // Discover the text formatter through the locator
  auto formatter = locator_->get<ITextFormatter>("text_formatter");
  if (!formatter) {
    throw std::runtime_error("ReportGenerator: text_formatter service not found");
  }

  // Compute stats via the stats_engine plugin
  double avg = stats->mean(data);
  double sd = stats->stddev(data);
  double med = stats->median(data);

  // Build the report
  std::ostringstream report;
  report << formatter->to_upper("data report") << "\n";
  report << formatter->reverse("---") << "---" << "\n";
  report << "  samples: " << data.size() << "\n";
  report << "  mean:    " << avg << "\n";
  report << "  stddev:  " << sd << "\n";
  report << "  median:  " << med << "\n";
  report << formatter->to_lower("END OF REPORT");

  return report.str();
}

}  // namespace examples

REGISTER_PLUGIN(examples::ReportGenerator)
