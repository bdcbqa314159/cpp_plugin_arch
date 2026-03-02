// C++/CLI bridge that wraps a .NET Framework assembly (ManagedMath.dll)
// behind the same pure C ABI used by dynamic_plugin_demo/math_functions.cpp.
//
// The host loads this DLL like any other native plugin — zero awareness of .NET.
//
// Compiled with /clr. References ManagedMath.dll via #using.

#using "ManagedMath.dll"

#include "PluginDescriptor.hpp"
#include "platform/clr_helpers.hpp"

#include <cstdlib>
#include <cstring>

using namespace System;
using namespace plugin_arch::platform;

// Module-level managed object — created on first call, lives until DLL unload.
// gcroot prevents the GC from collecting it while native code holds a reference.
static ManagedHandle<ManagedMath::Calculator> g_calc;

static ManagedMath::Calculator ^ get_calc() {
  if (!g_calc) {
    g_calc.reset(gcnew ManagedMath::Calculator());
  }
  return g_calc.get();
}

// ---------------------------------------------------------------------------
// Tier 1: Scalar blittable — ~5-10 ns/call overhead
// No marshaling needed: double is identical in native and managed memory.
// ---------------------------------------------------------------------------

EXPORT_C {

EXPORTED double math_add(double a, double b) {
  return get_calc()->Add(a, b);
}

EXPORTED double math_multiply(double a, double b) {
  return get_calc()->Multiply(a, b);
}

EXPORTED double math_sqrt(double a) {
  return get_calc()->Sqrt(a);
}

// ---------------------------------------------------------------------------
// Tier 2: Batch pinned — amortized over N elements
// Native double* can't be reinterpreted as managed double[].
// Copy in → call managed → pin_ptr + copy out.
// ---------------------------------------------------------------------------

EXPORTED void math_batch_multiply(double* values, int count, double factor) {
  // Copy native array into managed array
  array<double> ^ managed = gcnew array<double>(count);
  {
    pin_ptr<double> pinned = &managed[0];
    std::memcpy(pinned, values, count * sizeof(double));
  }

  // Call managed method (operates in-place on managed array)
  get_calc()->BatchMultiply(managed, factor);

  // Copy result back to native array
  {
    pin_ptr<double> pinned = &managed[0];
    std::memcpy(values, pinned, count * sizeof(double));
  }
}

// ---------------------------------------------------------------------------
// Tier 3: String marshal — ~100-500 ns/call overhead
// Returns malloc-owned C string. Caller must free via math_free_string().
// ---------------------------------------------------------------------------

EXPORTED char* math_format_result(const char* name, double value) {
  String ^ managed_name = marshal_to_managed(name);
  String ^ result = get_calc()->FormatResult(managed_name, value);
  std::string native = marshal_to_native(result);

  // Caller owns this memory — must call math_free_string() to release.
  char* buf = static_cast<char*>(std::malloc(native.size() + 1));
  if (buf) {
    std::memcpy(buf, native.c_str(), native.size() + 1);
  }
  return buf;
}

EXPORTED void math_free_string(char* str) {
  std::free(str);
}

}  // extern "C"

// ---------------------------------------------------------------------------
// Plugin descriptor — identical convention to dynamic_plugin_demo
// ---------------------------------------------------------------------------

REGISTER_DYNAMIC_PLUGIN("DotNetBridge", "1.0.0",
  {"math_add",            "double(double, double)"},
  {"math_multiply",       "double(double, double)"},
  {"math_sqrt",           "double(double)"},
  {"math_batch_multiply", "void(double*, int, double)"},
  {"math_format_result",  "char*(const char*, double)"},
  {"math_free_string",    "void(char*)"}
)
