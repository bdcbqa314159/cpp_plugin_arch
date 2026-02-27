#include "basic_calc.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

#include "PluginFactory.hpp"

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
  return std::numeric_limits<double>::quiet_NaN();  // not supported
}

double BasicCalc::sqrt(double /*a*/) {
  return std::numeric_limits<double>::quiet_NaN();  // not supported
}

}  // namespace examples

REGISTER_PLUGIN(examples::BasicCalc)
