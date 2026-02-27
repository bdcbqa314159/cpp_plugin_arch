// Example plugin interface: a calculator.
// Both basic_calc and scientific_calc implement this same interface,
// demonstrating polymorphism across shared library boundaries.

#pragma once

#include "IPlugin.hpp"

namespace examples {

class ICalculator : public plugin_arch::IPlugin {
 public:
  // Binary arithmetic
  virtual double add(double a, double b) = 0;
  virtual double subtract(double a, double b) = 0;
  virtual double multiply(double a, double b) = 0;
  virtual double divide(double a, double b) = 0;

  // Scientific (unary / binary)
  virtual double power(double base, double exponent) = 0;
  virtual double sqrt(double a) = 0;
};

}  // namespace examples
