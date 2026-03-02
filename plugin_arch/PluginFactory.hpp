// Macro to auto-generate the extern "C" allocator/deallocator boilerplate
// that every plugin needs. Usage:
//
//   REGISTER_PLUGIN(MyPluginClass)
//
// Expands to extern "C" functions: allocator() and deallocator(IPlugin*).

#pragma once

#include "IPlugin.hpp"
#include "platform/abi.hpp"
#include "platform/exported.hpp"

// The factory functions traffic in IPlugin* — the framework's root type.
// This avoids pointer-adjustment issues with multiple inheritance and keeps
// the C ABI contract explicit. delete on IPlugin* works because IPlugin has
// a virtual destructor.
#define REGISTER_PLUGIN(ClassName)                        \
  EXPORT_C {                                              \
    EXPORTED plugin_arch::IPlugin* allocator() {          \
      return new ClassName();                             \
    }                                                     \
    EXPORTED void deallocator(plugin_arch::IPlugin* ptr) {\
      delete ptr;                                         \
    }                                                     \
  }
