// Report generator plugin — uses the service locator to discover and call
// IStatsEngine and ITextFormatter plugins at runtime.
//
// This plugin never links against or includes the other plugins' source code.
// It only knows their interfaces.

#pragma once

#include "IReportGenerator.hpp"
#include "ServiceLocator.hpp"

namespace examples {

class ReportGenerator : public IReportGenerator, public plugin_arch::IServiceAware {
 public:
  // Plugin metadata
  const std::string& name() const override { static const std::string s("ReportGenerator"); return s; }
  const std::string& version() const override { static const std::string s("1.0.0"); return s; }
  const std::string& type() const override { static const std::string s("report_generator"); return s; }

  // IServiceAware — the host injects the locator before calling generate()
  void set_service_locator(plugin_arch::ServiceLocator& locator) override;

  // IReportGenerator — uses locator internally to find other plugins
  std::string generate(const std::vector<double>& data) override;

 private:
  plugin_arch::ServiceLocator* locator_ = nullptr;
};

}  // namespace examples
