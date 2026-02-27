// Interface extracted from libA's public methods.
// This is the only file the host needs to see.

#pragma once

#include <string>

#include "IPlugin.hpp"

namespace examples {

class ITextFormatter : public plugin_arch::IPlugin {
 public:
  virtual std::string to_upper(const std::string& text) = 0;
  virtual std::string to_lower(const std::string& text) = 0;
  virtual std::string reverse(const std::string& text) = 0;
};

}  // namespace examples
