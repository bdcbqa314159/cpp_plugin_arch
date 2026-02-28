// POD structs + macro for plugins that export a pure C ABI with no headers.
// Plugins use REGISTER_DYNAMIC_PLUGIN() to export a plugin_describe() function
// that returns a static descriptor listing all available functions.
//
// Usage (in a plugin .cpp with no interface header):
//
//   REGISTER_DYNAMIC_PLUGIN("MyPlugin", "1.0.0",
//     {"my_func",     "int(int, int)"},
//     {"my_other",    "void()"}
//   )

#pragma once

#include "platform/abi.hpp"
#include "platform/exported.hpp"

namespace plugin_arch {

struct FunctionEntry {
  const char* name;       // e.g. "math_add"
  const char* signature;  // e.g. "double(double, double)" — informational only
};

struct PluginDescriptor {
  const char* name;
  const char* version;
  int function_count;
  const FunctionEntry* functions;
};

}  // namespace plugin_arch

#define REGISTER_DYNAMIC_PLUGIN(plugin_name, plugin_version, ...)              \
  EXPORT_C {                                                                   \
    EXPORTED const plugin_arch::PluginDescriptor* plugin_describe() {          \
      static const plugin_arch::FunctionEntry entries[] = {__VA_ARGS__};       \
      static const plugin_arch::PluginDescriptor descriptor = {                \
          plugin_name,                                                         \
          plugin_version,                                                      \
          static_cast<int>(                                                    \
              sizeof(entries) / sizeof(plugin_arch::FunctionEntry)),            \
          entries};                                                            \
      return &descriptor;                                                      \
    }                                                                          \
  }
