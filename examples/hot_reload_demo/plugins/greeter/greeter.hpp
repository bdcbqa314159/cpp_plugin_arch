#pragma once

#include "IGreeter.hpp"

namespace examples {

class Greeter : public IGreeter {
 public:
  const std::string& name() const override { static const std::string s("GreeterPlugin"); return s; }
  const std::string& version() const override { static const std::string s("1.0.0"); return s; }
  const std::string& type() const override { static const std::string s("greeter"); return s; }

  std::string greet() override;
};

}  // namespace examples
