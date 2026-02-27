// RAII cross-platform dynamic library loader.
// Opens on construction, closes on destruction.
// Returns shared_ptr<T> with a custom deleter that calls back into the plugin.

#pragma once

#include <memory>
#include <stdexcept>
#include <string>

#if defined(_WIN32)
  #include <Windows.h>
#elif defined(__linux__) || defined(__APPLE__)
  #include <dlfcn.h>
#endif

namespace plugin_arch {

template <typename T>
class PluginLoader {
 public:
  PluginLoader(const std::string& library_path,
               const std::string& alloc_symbol = "allocator",
               const std::string& dealloc_symbol = "deallocator")
      : library_path_(library_path),
        alloc_symbol_(alloc_symbol),
        dealloc_symbol_(dealloc_symbol) {
    open();
  }

  ~PluginLoader() { close(); }

  PluginLoader(const PluginLoader&) = delete;
  PluginLoader& operator=(const PluginLoader&) = delete;

  PluginLoader(PluginLoader&& other) noexcept
      : handle_(other.handle_),
        library_path_(std::move(other.library_path_)),
        alloc_symbol_(std::move(other.alloc_symbol_)),
        dealloc_symbol_(std::move(other.dealloc_symbol_)) {
    other.handle_ = nullptr;
  }

  PluginLoader& operator=(PluginLoader&& other) noexcept {
    if (this != &other) {
      close();
      handle_ = other.handle_;
      library_path_ = std::move(other.library_path_);
      alloc_symbol_ = std::move(other.alloc_symbol_);
      dealloc_symbol_ = std::move(other.dealloc_symbol_);
      other.handle_ = nullptr;
    }
    return *this;
  }

  std::shared_ptr<T> get_instance() {
    using AllocFunc = T* (*)();
    using DeallocFunc = void (*)(T*);

    auto alloc = resolve_symbol<AllocFunc>(alloc_symbol_);
    auto dealloc = resolve_symbol<DeallocFunc>(dealloc_symbol_);

    T* raw = alloc();
    if (!raw) {
      throw std::runtime_error("allocator returned nullptr for: " +
                               library_path_);
    }

    return std::shared_ptr<T>(raw, [dealloc](T* ptr) { dealloc(ptr); });
  }

 private:
#if defined(_WIN32)
  using Handle = HMODULE;
#else
  using Handle = void*;
#endif

  Handle handle_ = nullptr;
  std::string library_path_;
  std::string alloc_symbol_;
  std::string dealloc_symbol_;

  void open() {
#if defined(_WIN32)
    handle_ = LoadLibrary(library_path_.c_str());
    if (!handle_) {
      throw std::runtime_error("Failed to load library: " + library_path_);
    }
#elif defined(__linux__) || defined(__APPLE__)
    handle_ = dlopen(library_path_.c_str(), RTLD_NOW);
    if (!handle_) {
      throw std::runtime_error("Failed to load library: " +
                               std::string(dlerror()));
    }
#endif
  }

  void close() {
    if (!handle_) return;
#if defined(_WIN32)
    FreeLibrary(handle_);
#elif defined(__linux__) || defined(__APPLE__)
    dlclose(handle_);
#endif
    handle_ = nullptr;
  }

  template <typename Func>
  Func resolve_symbol(const std::string& symbol) {
#if defined(_WIN32)
    auto func = reinterpret_cast<Func>(GetProcAddress(handle_, symbol.c_str()));
#elif defined(__linux__) || defined(__APPLE__)
    auto func = reinterpret_cast<Func>(dlsym(handle_, symbol.c_str()));
#endif
    if (!func) {
      throw std::runtime_error("Failed to resolve symbol '" + symbol +
                               "' in: " + library_path_);
    }
    return func;
  }
};

}  // namespace plugin_arch
