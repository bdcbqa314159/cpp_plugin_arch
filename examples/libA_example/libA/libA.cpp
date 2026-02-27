// libA implementation.
// These method bodies are 100% unchanged from the original library.
// The only addition is REGISTER_PLUGIN() at the bottom.

#include "libA.hpp"

#include <algorithm>

#include "PluginFactory.hpp"

namespace examples {

std::string LibA::to_upper(const std::string& text) {
  std::string result = text;
  std::transform(result.begin(), result.end(), result.begin(), ::toupper);
  return result;
}

std::string LibA::to_lower(const std::string& text) {
  std::string result = text;
  std::transform(result.begin(), result.end(), result.begin(), ::tolower);
  return result;
}

std::string LibA::reverse(const std::string& text) {
  return std::string(text.rbegin(), text.rend());
}

}  // namespace examples

// This one line is all that was added to make libA a plugin.
REGISTER_PLUGIN(examples::LibA)
