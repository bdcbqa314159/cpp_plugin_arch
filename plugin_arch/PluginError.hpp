// Structured error hierarchy for the plugin framework.
//
// All types inherit std::runtime_error — existing catch(const std::exception&)
// handlers continue to work unchanged.
//
//   std::runtime_error
//     └── PluginError              (base — carries library path)
//           ├── LoadError          (dlopen/LoadLibrary failed — platform error string)
//           ├── SymbolError        (dlsym/GetProcAddress failed — symbol name)
//           ├── AllocationError    (allocator returned nullptr)
//           └── InterfaceError     (dynamic_cast failed — requested type name)

#pragma once

#include <stdexcept>
#include <string>

namespace plugin_arch {

// Base error for all plugin-related failures. Carries the library path.
class PluginError : public std::runtime_error {
 public:
  PluginError(std::string library_path, const std::string& message)
      : std::runtime_error(message),
        library_path_(std::move(library_path)) {}

  [[nodiscard]] const std::string& library_path() const { return library_path_; }

 private:
  std::string library_path_;
};

// dlopen / LoadLibrary failed. Carries the platform-specific error string.
// Always construct via ::make() to avoid reading moved-from state.
class LoadError : public PluginError {
 public:
  static LoadError make(std::string library_path, std::string platform_error) {
    std::string msg = "Failed to load library: " + library_path +
                      " (" + platform_error + ")";
    return LoadError(std::move(library_path), std::move(platform_error),
                     std::move(msg));
  }

  [[nodiscard]] const std::string& platform_error() const { return platform_error_; }

 private:
  LoadError(std::string library_path, std::string platform_error, std::string message)
      : PluginError(std::move(library_path), message),
        platform_error_(std::move(platform_error)) {}

  std::string platform_error_;
};

// dlsym / GetProcAddress failed. Carries the symbol name that was not found.
class SymbolError : public PluginError {
 public:
  static SymbolError make(std::string library_path, std::string symbol_name,
                          std::string platform_error) {
    std::string msg = "Failed to resolve symbol '" + symbol_name +
                      "' in: " + library_path + " (" + platform_error + ")";
    return SymbolError(std::move(library_path), std::move(symbol_name),
                       std::move(platform_error), std::move(msg));
  }

  [[nodiscard]] const std::string& symbol_name() const { return symbol_name_; }
  [[nodiscard]] const std::string& platform_error() const { return platform_error_; }

 private:
  SymbolError(std::string library_path, std::string symbol_name,
              std::string platform_error, std::string message)
      : PluginError(std::move(library_path), message),
        symbol_name_(std::move(symbol_name)),
        platform_error_(std::move(platform_error)) {}

  std::string symbol_name_;
  std::string platform_error_;
};

// allocator() returned nullptr.
class AllocationError : public PluginError {
 public:
  static AllocationError make(std::string library_path) {
    std::string msg = "allocator returned nullptr for: " + library_path;
    return AllocationError(std::move(library_path), std::move(msg));
  }

 private:
  AllocationError(std::string library_path, std::string message)
      : PluginError(std::move(library_path), message) {}
};

// dynamic_cast to requested interface failed. Carries the requested type name.
class InterfaceError : public PluginError {
 public:
  static InterfaceError make(std::string library_path,
                             std::string requested_type) {
    std::string msg = "plugin does not implement requested interface '" +
                      requested_type + "': " + library_path;
    return InterfaceError(std::move(library_path), std::move(requested_type),
                          std::move(msg));
  }

  [[nodiscard]] const std::string& requested_type() const { return requested_type_; }

 private:
  InterfaceError(std::string library_path, std::string requested_type,
                 std::string message)
      : PluginError(std::move(library_path), message),
        requested_type_(std::move(requested_type)) {}

  std::string requested_type_;
};

}  // namespace plugin_arch
