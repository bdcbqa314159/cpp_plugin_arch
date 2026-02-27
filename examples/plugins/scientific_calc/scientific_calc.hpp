#pragma once

#include "ICalculator.hpp"

namespace examples {

class ScientificCalc : public ICalculator {
 public:
  std::string name() const override { return "ScientificCalc"; }
  std::string version() const override { return "1.0.0"; }
  std::string type() const override { return "calculator"; }

  double add(double a, double b) override;
  double subtract(double a, double b) override;
  double multiply(double a, double b) override;
  double divide(double a, double b) override;

  double power(double base, double exponent) override;
  double sqrt(double a) override;
};

}  // namespace examples
