#pragma once

#include "ICalculator.hpp"

namespace examples {

class ScientificCalc : public ICalculator {
 public:
  const std::string& name() const override { static const std::string s("ScientificCalc"); return s; }
  const std::string& version() const override { static const std::string s("1.0.0"); return s; }
  const std::string& type() const override { static const std::string s("calculator"); return s; }

  double add(double a, double b) override;
  double subtract(double a, double b) override;
  double multiply(double a, double b) override;
  double divide(double a, double b) override;

  double power(double base, double exponent) override;
  double sqrt(double a) override;
};

}  // namespace examples
