#include "trig_calc.hpp"

#include <cmath>
#include <stdexcept>

#include "PluginExport.hpp"

namespace examples {

// --- v1 (ICalculator) ---

double TrigCalc::add(double a, double b) { return a + b; }

double TrigCalc::subtract(double a, double b) { return a - b; }

double TrigCalc::multiply(double a, double b) { return a * b; }

double TrigCalc::divide(double a, double b) {
  if (b == 0.0) {
    throw std::domain_error("TrigCalc: division by zero");
  }
  return a / b;
}

double TrigCalc::power(double base, double exponent) {
  return std::pow(base, exponent);
}

double TrigCalc::sqrt(double a) {
  if (a < 0.0) {
    throw std::domain_error("TrigCalc: sqrt of negative number");
  }
  return std::sqrt(a);
}

// --- v2 (ICalculatorV2) ---

double TrigCalc::sin(double a) { return std::sin(a); }

double TrigCalc::cos(double a) { return std::cos(a); }

double TrigCalc::log(double a) {
  if (a <= 0.0) {
    throw std::domain_error("TrigCalc: log of non-positive number");
  }
  return std::log(a);
}

}  // namespace examples

REGISTER_PLUGIN(examples::TrigCalc)
