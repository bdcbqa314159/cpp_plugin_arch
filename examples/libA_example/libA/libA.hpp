// libA — a simple text formatter library.
// This is the class we want to make loadable as a plugin.
//
// BEFORE plugin-ification, this was just:
//
//   class LibA {
//    public:
//     std::string to_upper(const std::string& text);
//     std::string to_lower(const std::string& text);
//     std::string reverse(const std::string& text);
//   };
//
// What changed:
//   1. Added ": public ITextFormatter" (the interface)
//   2. Added name(), version(), type() overrides
//   3. Method bodies stayed exactly the same

#pragma once

#include "ITextFormatter.hpp"

namespace examples {

class LibA : public ITextFormatter {
 public:
  // -- plugin metadata (added for plugin_arch) --
  std::string name() const override { return "LibA"; }
  std::string version() const override { return "1.0.0"; }
  std::string type() const override { return "text_formatter"; }

  // -- original methods (unchanged) --
  std::string to_upper(const std::string& text) override;
  std::string to_lower(const std::string& text) override;
  std::string reverse(const std::string& text) override;
};

}  // namespace examples
