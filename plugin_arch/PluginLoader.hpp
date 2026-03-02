// Typed RAII plugin loader.
// Composes DynamicLibrary for the actual dlopen/dlclose/dlsym work.
// Returns shared_ptr<T> with a custom deleter that calls back into the plugin.
//
// Symbols are resolved at construction time — the constructor throws if the
// library doesn't export the expected allocator/deallocator functions.

#pragma once

#include <memory>
#include <stdexcept>
#include <string>

#include "DynamicLibrary.hpp"

namespace plugin_arch {

template <typename T>
class PluginLoader {
 public:
  using AllocFunc = T* (*)();
  using DeallocFunc = void (*)(T*);

  explicit PluginLoader(const std::string& library_path,
               const std::string& alloc_symbol = "allocator",
               const std::string& dealloc_symbol = "deallocator")
      : lib_(std::make_shared<DynamicLibrary>(library_path)),
        alloc_(lib_->resolve<AllocFunc>(alloc_symbol)),
        dealloc_(lib_->resolve<DeallocFunc>(dealloc_symbol)) {}

  PluginLoader(const PluginLoader&) = delete;
  PluginLoader& operator=(const PluginLoader&) = delete;

  PluginLoader(PluginLoader&&) noexcept = default;
  PluginLoader& operator=(PluginLoader&&) noexcept = default;

  std::shared_ptr<T> get_instance() {
    T* raw = alloc_();
    if (!raw) {
      throw std::runtime_error("allocator returned nullptr for: " +
                               lib_->path());
    }

    // Capture a shared reference to the library so it stays loaded as long
    // as the plugin instance exists — the deleter calls back into library code.
    auto lib_ref = lib_;
    auto dealloc = dealloc_;
    return std::shared_ptr<T>(raw,
                              [dealloc, lib_ref](T* ptr) { dealloc(ptr); });
  }

  const std::string& path() const { return lib_->path(); }

 private:
  std::shared_ptr<DynamicLibrary> lib_;
  AllocFunc alloc_;
  DeallocFunc dealloc_;
};

}  // namespace plugin_arch
