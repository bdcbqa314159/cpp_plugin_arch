#pragma once

#include "IGreeter.hpp"

namespace examples {

class Greeter : public IGreeter {
 public:
  std::string name() const override { return "GreeterPlugin"; }
  std::string version() const override { return "1.0.0"; }
  std::string type() const override { return "greeter"; }

  std::string greet() override;
};

}  // namespace examples
