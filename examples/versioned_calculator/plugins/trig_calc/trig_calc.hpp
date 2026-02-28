#pragma once

#include "ICalculatorV2.hpp"

namespace examples {

class TrigCalc : public ICalculatorV2 {
 public:
  std::string name() const override { return "TrigCalc"; }
  std::string version() const override { return "2.0.0"; }
  std::string type() const override { return "calculator"; }

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
