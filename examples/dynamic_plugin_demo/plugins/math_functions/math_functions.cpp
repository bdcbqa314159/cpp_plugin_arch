// A plugin with no interface header — pure C ABI.
// The host discovers these functions at runtime via plugin_describe().

#include "PluginDescriptor.hpp"

#include <cmath>

EXPORT_C {

EXPORTED double math_add(double a, double b) { return a + b; }

EXPORTED double math_multiply(double a, double b) { return a * b; }

EXPORTED double math_sqrt(double a) { return std::sqrt(a); }

}  // extern "C"

REGISTER_DYNAMIC_PLUGIN("MathFunctions", "1.0.0",
  {"math_add",      "double(double, double)"},
  {"math_multiply", "double(double, double)"},
  {"math_sqrt",     "double(double)"}
)
