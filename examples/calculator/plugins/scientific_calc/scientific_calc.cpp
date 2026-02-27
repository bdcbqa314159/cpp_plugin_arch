#include "scientific_calc.hpp"

#include <cmath>
#include <stdexcept>

#include "PluginFactory.hpp"

namespace examples {

double ScientificCalc::add(double a, double b) { return a + b; }

double ScientificCalc::subtract(double a, double b) { return a - b; }

double ScientificCalc::multiply(double a, double b) { return a * b; }

double ScientificCalc::divide(double a, double b) {
  if (b == 0.0) {
    throw std::domain_error("ScientificCalc: division by zero");
  }
  return a / b;
}

double ScientificCalc::power(double base, double exponent) {
  return std::pow(base, exponent);
}

double ScientificCalc::sqrt(double a) {
  if (a < 0.0) {
    throw std::domain_error("ScientificCalc: sqrt of negative number");
  }
  return std::sqrt(a);
}

}  // namespace examples

REGISTER_PLUGIN(examples::ScientificCalc)
