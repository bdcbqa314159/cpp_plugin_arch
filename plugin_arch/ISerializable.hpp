// Opt-in mixin for plugins that can save and restore state.
//
// HotPluginLoader uses this to preserve state across reloads:
//   1. Call save_state() on old instance
//   2. Reload library, create new instance
//   3. Call restore_state() on new instance with saved data
//
// The framework doesn't mandate a serialization format — plugins choose
// their own (JSON, protobuf, raw bytes, etc.).

#pragma once

#include <string>

namespace plugin_arch {

class ISerializable {
 public:
  virtual ~ISerializable() = default;

  // Serialize current state to a byte string.
  virtual std::string save_state() const = 0;

  // Restore state from a previously saved byte string.
  virtual void restore_state(const std::string& data) = 0;
};

}  // namespace plugin_arch
