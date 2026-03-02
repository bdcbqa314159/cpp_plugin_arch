// CLR interop utilities for C++/CLI bridge plugins.
// Only active when compiled with /clr (MSVC). No-op otherwise.
//
// Follows the convention of abi.hpp, exported.hpp, shared_lib.hpp.

#pragma once

#if defined(__cplusplus_cli)

#include <msclr/gcroot.h>
#include <string>

namespace plugin_arch::platform {

// RAII wrapper around msclr::gcroot<T^>.
// Prevents the GC from collecting a managed object held from native code.
// Non-copyable, move-friendly — same semantics as DynamicLibrary.
//
// Usage:
//   ManagedHandle<Calculator> calc;
//   calc.reset(gcnew ManagedMath::Calculator());
//   calc->Add(1, 2);
//
template <typename T>
class ManagedHandle {
 public:
  ManagedHandle() = default;

  explicit ManagedHandle(T ^ obj) : handle_(obj) {}

  ~ManagedHandle() { handle_ = nullptr; }

  ManagedHandle(const ManagedHandle&) = delete;
  ManagedHandle& operator=(const ManagedHandle&) = delete;

  ManagedHandle(ManagedHandle&& other) noexcept : handle_(other.handle_) {
    other.handle_ = nullptr;
  }

  ManagedHandle& operator=(ManagedHandle&& other) noexcept {
    if (this != &other) {
      handle_ = other.handle_;
      other.handle_ = nullptr;
    }
    return *this;
  }

  void reset(T ^ obj) { handle_ = obj; }

  T ^ get() { return static_cast<T ^>(handle_); }
  T ^ operator->() { return get(); }
  explicit operator bool() const {
    return static_cast<T ^>(handle_) != nullptr;
  }

 private:
  msclr::gcroot<T ^> handle_;
};

// --- Marshaling helpers ---

// Convert managed String^ to std::string (UTF-8).
inline std::string marshal_to_native(System::String ^ str) {
  if (str == nullptr) return {};
  using namespace System::Runtime::InteropServices;
  System::IntPtr ptr = Marshal::StringToHGlobalAnsi(str);
  try {
    std::string result(static_cast<const char*>(ptr.ToPointer()));
    Marshal::FreeHGlobal(ptr);
    return result;
  } catch (...) {
    Marshal::FreeHGlobal(ptr);
    throw;
  }
}

// Convert C string to managed String^.
inline System::String ^ marshal_to_managed(const char* str) {
  if (!str) return nullptr;
  return gcnew System::String(str);
}

// --- pin_ptr recipe (for reference) ---
//
// pin_ptr<T> pins a managed object so its address won't change during GC.
// It is stack-only and cannot be stored in a class member.
//
// Example — copying native doubles into a managed array:
//
//   array<double>^ managed = gcnew array<double>(count);
//   {
//       pin_ptr<double> pinned = &managed[0];
//       memcpy(pinned, native_ptr, count * sizeof(double));
//   }
//   // pinned goes out of scope — object is unpinned
//
// Example — copying managed array back to native:
//
//   {
//       pin_ptr<double> pinned = &managed[0];
//       memcpy(native_ptr, pinned, count * sizeof(double));
//   }

}  // namespace plugin_arch::platform

#endif  // __cplusplus_cli
