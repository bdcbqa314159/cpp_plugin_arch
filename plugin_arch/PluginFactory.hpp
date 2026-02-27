// Macro to auto-generate the extern "C" allocator/deallocator boilerplate
// that every plugin needs. Usage:
//
//   REGISTER_PLUGIN(MyPluginClass)
//
// Expands to extern "C" functions: allocator() and deallocator(ClassName*).

#pragma once

#include "platform/abi.hpp"
#include "platform/exported.hpp"

#define REGISTER_PLUGIN(ClassName)                        \
  EXPORT_C {                                              \
    EXPORTED ClassName* allocator() {                     \
      return new ClassName();                             \
    }                                                     \
    EXPORTED void deallocator(ClassName* ptr) {           \
      delete ptr;                                         \
    }                                                     \
  }
