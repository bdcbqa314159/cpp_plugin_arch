// Extended calculator interface (v2).
//
// ICalculatorV2 adds trig and log methods on top of ICalculator.
// Existing v1 plugins (BasicCalc, ScientificCalc) are untouched — they
// implement only ICalculator.  The host uses dynamic_cast to detect
// which version a loaded plugin supports.
//
// Key ABI insight: single inheritance chain
//   IPlugin -> ICalculator -> ICalculatorV2
// The ICalculator portion of the vtable sits at the same offsets
// regardless of whether the concrete class stops at v1 or continues
// to v2.  dynamic_cast uses RTTI to safely detect the extension.

#pragma once

#include "ICalculator.hpp"

namespace examples {

class ICalculatorV2 : public ICalculator {
 public:
  virtual double sin(double a) = 0;
  virtual double cos(double a) = 0;
  virtual double log(double a) = 0;
};

}  // namespace examples
