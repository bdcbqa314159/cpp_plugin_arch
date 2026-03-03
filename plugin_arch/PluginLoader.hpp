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
#include "IPlugin.hpp"

namespace plugin_arch {

template <typename T>
class PluginLoader {
 public:
  // Factory functions always traffic in IPlugin* — the framework's root type.
  // This avoids pointer-adjustment issues with multiple inheritance.
  using AllocFunc = IPlugin* (*)();
  using DeallocFunc = void (*)(IPlugin*);

  explicit PluginLoader(std::string library_path,
               std::string alloc_symbol = "allocator",
               std::string dealloc_symbol = "deallocator")
      : lib_(std::make_shared<DynamicLibrary>(std::move(library_path))),
        alloc_(lib_->resolve<AllocFunc>(alloc_symbol)),
        dealloc_(lib_->resolve<DeallocFunc>(dealloc_symbol)) {}

  PluginLoader(const PluginLoader&) = delete;
  PluginLoader& operator=(const PluginLoader&) = delete;

  PluginLoader(PluginLoader&&) noexcept = default;
  PluginLoader& operator=(PluginLoader&&) noexcept = default;

  [[nodiscard]] std::shared_ptr<T> get_instance() {
    IPlugin* raw = alloc_();
    if (!raw) {
      throw std::runtime_error("allocator returned nullptr for: " +
                               lib_->path());
    }

    T* typed = dynamic_cast<T*>(raw);
    if (!typed) {
      dealloc_(raw);
      throw std::runtime_error("plugin does not implement requested interface: " +
                               lib_->path());
    }

    // Capture a shared reference to the library so it stays loaded as long
    // as the plugin instance exists — the deleter calls back into library code.
    // The deleter uses the original IPlugin* (not the casted T*) to avoid
    // pointer-adjustment issues with multiple inheritance.
    auto lib_ref = lib_;
    auto dealloc = dealloc_;
    return std::shared_ptr<T>(typed,
                              [raw, dealloc, lib_ref](T*) { dealloc(raw); });
  }

  [[nodiscard]] const std::string& path() const { return lib_->path(); }

 private:
  std::shared_ptr<DynamicLibrary> lib_;
  AllocFunc alloc_;
  DeallocFunc dealloc_;
};

}  // namespace plugin_arch
