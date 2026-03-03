// Non-templated RAII wrapper around dlopen/dlclose.
// Unlike PluginLoader<T>, makes no assumptions about what symbols exist
// or what types they return. The caller provides function pointer types
// when resolving symbols.

#pragma once

#include <stdexcept>
#include <string>
#include <type_traits>

#if defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <Windows.h>
#elif defined(__linux__) || defined(__APPLE__)
  #include <dlfcn.h>
#else
  #error "DynamicLibrary: unsupported platform"
#endif

namespace plugin_arch {

class DynamicLibrary {
 public:
  explicit DynamicLibrary(std::string library_path)
      : library_path_(std::move(library_path)) {
    open();
  }

  ~DynamicLibrary() { close(); }

  DynamicLibrary(const DynamicLibrary&) = delete;
  DynamicLibrary& operator=(const DynamicLibrary&) = delete;

  DynamicLibrary(DynamicLibrary&& other) noexcept
      : handle_(other.handle_),
        library_path_(std::move(other.library_path_)) {
    other.handle_ = nullptr;
  }

  DynamicLibrary& operator=(DynamicLibrary&& other) noexcept {
    if (this != &other) {
      close();
      handle_ = other.handle_;
      library_path_ = std::move(other.library_path_);
      other.handle_ = nullptr;
    }
    return *this;
  }

  // Resolve a symbol by name and cast to the given function pointer type.
  // Throws std::runtime_error if the symbol is not found.
  template <typename Func>
  [[nodiscard]] Func resolve(const std::string& symbol) const {
    static_assert(std::is_pointer_v<Func> &&
                      std::is_function_v<std::remove_pointer_t<Func>>,
                  "resolve<Func>: Func must be a function pointer type");
#if defined(_WIN32)
    auto func =
        reinterpret_cast<Func>(GetProcAddress(handle_, symbol.c_str()));
    if (!func) {
      auto err = GetLastError();
      throw std::runtime_error("Failed to resolve symbol '" + symbol +
                               "' in: " + library_path_ +
                               " (error " + std::to_string(err) + ")");
    }
    return func;
#elif defined(__linux__) || defined(__APPLE__)
    (void)dlerror();  // clear any previous error
    // POSIX guarantees void* <-> function pointer conversion works with dlsym.
    auto func = reinterpret_cast<Func>(dlsym(handle_, symbol.c_str()));
    const char* err = dlerror();
    if (err) {
      throw std::runtime_error("Failed to resolve symbol '" + symbol +
                               "' in: " + library_path_ + " (" + err + ")");
    }
    return func;
#endif
  }

  // Check whether a symbol exists without throwing.
  [[nodiscard]] bool has(const std::string& symbol) const {
#if defined(_WIN32)
    return GetProcAddress(handle_, symbol.c_str()) != nullptr;
#elif defined(__linux__) || defined(__APPLE__)
    (void)dlerror();  // clear any previous error
    (void)dlsym(handle_, symbol.c_str());
    return dlerror() == nullptr;
#endif
  }

  [[nodiscard]] const std::string& path() const { return library_path_; }

 private:
#if defined(_WIN32)
  using Handle = HMODULE;
#else
  using Handle = void*;
#endif

  Handle handle_ = nullptr;
  std::string library_path_;

  void open() {
#if defined(_WIN32)
    handle_ = LoadLibraryA(library_path_.c_str());
    if (!handle_) {
      auto err = GetLastError();
      throw std::runtime_error("Failed to load library: " + library_path_ +
                               " (error " + std::to_string(err) + ")");
    }
#elif defined(__linux__) || defined(__APPLE__)
    handle_ = dlopen(library_path_.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle_) {
      const char* err = dlerror();
      throw std::runtime_error("Failed to load library: " + library_path_ +
                               " (" + (err ? err : "unknown error") + ")");
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
};

}  // namespace plugin_arch
