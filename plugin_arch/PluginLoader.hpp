// Typed RAII plugin loader.
// Composes DynamicLibrary for the actual dlopen/dlclose/dlsym work.
// Returns shared_ptr<T> with a custom deleter that calls back into the plugin.

#pragma once

#include <memory>
#include <stdexcept>
#include <string>

#include "DynamicLibrary.hpp"

namespace plugin_arch {

template <typename T>
class PluginLoader {
 public:
  PluginLoader(const std::string& library_path,
               const std::string& alloc_symbol = "allocator",
               const std::string& dealloc_symbol = "deallocator")
      : lib_(library_path),
        alloc_symbol_(alloc_symbol),
        dealloc_symbol_(dealloc_symbol) {}

  PluginLoader(const PluginLoader&) = delete;
  PluginLoader& operator=(const PluginLoader&) = delete;

  PluginLoader(PluginLoader&&) noexcept = default;
  PluginLoader& operator=(PluginLoader&&) noexcept = default;

  std::shared_ptr<T> get_instance() {
    using AllocFunc = T* (*)();
    using DeallocFunc = void (*)(T*);

    auto alloc = lib_.resolve<AllocFunc>(alloc_symbol_);
    auto dealloc = lib_.resolve<DeallocFunc>(dealloc_symbol_);

    T* raw = alloc();
    if (!raw) {
      throw std::runtime_error("allocator returned nullptr for: " +
                               lib_.path());
    }

    return std::shared_ptr<T>(raw, [dealloc](T* ptr) { dealloc(ptr); });
  }

  const std::string& path() const { return lib_.path(); }

 private:
  DynamicLibrary lib_;
  std::string alloc_symbol_;
  std::string dealloc_symbol_;
};

}  // namespace plugin_arch
