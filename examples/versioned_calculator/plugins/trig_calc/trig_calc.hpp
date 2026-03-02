#pragma once

#include "ICalculatorV2.hpp"

namespace examples {

class TrigCalc : public ICalculatorV2 {
 public:
  const std::string& name() const override { static const std::string s("TrigCalc"); return s; }
  const std::string& version() const override { static const std::string s("2.0.0"); return s; }
  const std::string& type() const override { static const std::string s("calculator"); return s; }

  // v1 (ICalculator)
  double add(double a, double b) override;
  double subtract(double a, double b) override;
  double multiply(double a, double b) override;
  double divide(double a, double b) override;
  double power(double base, double exponent) override;
  double sqrt(double a) override;

  // v2 (ICalculatorV2)
  double sin(double a) override;
  double cos(double a) override;
  double log(double a) override;
};

}  // namespace examples
