#include "basic_calc.hpp"

#include <stdexcept>

#include "PluginExport.hpp"

namespace examples {

double BasicCalc::add(double a, double b) { return a + b; }

double BasicCalc::subtract(double a, double b) { return a - b; }

double BasicCalc::multiply(double a, double b) { return a * b; }

double BasicCalc::divide(double a, double b) {
  if (b == 0.0) {
    throw std::domain_error("BasicCalc: division by zero");
  }
  return a / b;
}

double BasicCalc::power(double /*base*/, double /*exponent*/) {
  throw std::domain_error("BasicCalc: power() not supported");
}

double BasicCalc::sqrt(double /*a*/) {
  throw std::domain_error("BasicCalc: sqrt() not supported");
}

}  // namespace examples

REGISTER_PLUGIN(examples::BasicCalc)
