// Plugin registry — scans a directory for shared libraries, probes each one
// for IPlugin metadata, and indexes the results. The host queries by type or
// name instead of relying on filename conventions.
//
// The registry indexes only. The host still uses PluginLoader<T> to do the
// actual loading when it wants to use a plugin.

#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "IPlugin.hpp"
#include "platform/shared_lib.hpp"

#if defined(_WIN32)
  #include <Windows.h>
#elif defined(__linux__) || defined(__APPLE__)
  #include <dlfcn.h>
#endif

namespace plugin_arch {

struct PluginEntry {
  std::filesystem::path path;
  std::string name;
  std::string version;
  std::string type;
};

struct ScanError {
  std::filesystem::path path;
  std::string reason;
};

class PluginRegistry {
 public:
  // Scan a directory for shared libraries and probe each one for metadata.
  // Appends to existing entries, so you can scan multiple directories.
  // Returns the number of new plugins discovered in this call.
  std::size_t scan(const std::filesystem::path& directory) {
    namespace fs = std::filesystem;

    if (!fs::is_directory(directory)) {
      throw std::runtime_error("Not a directory: " + directory.string());
    }

    std::size_t found = 0;

    for (const auto& entry : fs::directory_iterator(directory)) {
      if (!entry.is_regular_file()) continue;
      if (!platform::is_shared_library(entry.path())) continue;

      try {
        auto plugin_entry = probe(entry.path());
        entries_.push_back(std::move(plugin_entry));
        ++found;
      } catch (const std::exception& e) {
        errors_.push_back({entry.path(), e.what()});
      }
    }

    return found;
  }

  // Get all plugins of a given type.
  std::vector<PluginEntry> get_all(const std::string& type) const {
    std::vector<PluginEntry> result;
    for (const auto& e : entries_) {
      if (e.type == type) {
        result.push_back(e);
      }
    }
    return result;
  }

  // Get a plugin by name. Returns the first match.
  std::optional<PluginEntry> get(const std::string& name) const {
    for (const auto& e : entries_) {
      if (e.name == name) {
        return e;
      }
    }
    return std::nullopt;
  }

  const std::vector<PluginEntry>& entries() const { return entries_; }
  const std::vector<ScanError>& errors() const { return errors_; }

  void clear() {
    entries_.clear();
    errors_.clear();
  }

 private:
  std::vector<PluginEntry> entries_;
  std::vector<ScanError> errors_;

  // RAII guard that calls a cleanup function on scope exit.
  class ScopeGuard {
   public:
    explicit ScopeGuard(std::function<void()> fn) : fn_(std::move(fn)) {}
    ~ScopeGuard() { if (fn_) fn_(); }
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
   private:
    std::function<void()> fn_;
  };

  // Probe a single shared library for plugin metadata.
  static PluginEntry probe(const std::filesystem::path& path) {
#if defined(_WIN32)
    HMODULE handle = LoadLibrary(path.string().c_str());
    if (!handle) {
      throw std::runtime_error("Failed to load library");
    }
    ScopeGuard guard([&] { FreeLibrary(handle); });

    auto alloc = reinterpret_cast<IPlugin* (*)()>(
        GetProcAddress(handle, "allocator"));
    if (!alloc) {
      throw std::runtime_error("Symbol 'allocator' not found");
    }

    auto dealloc = reinterpret_cast<void (*)(IPlugin*)>(
        GetProcAddress(handle, "deallocator"));
    if (!dealloc) {
      throw std::runtime_error("Symbol 'deallocator' not found");
    }
#elif defined(__linux__) || defined(__APPLE__)
    void* handle = dlopen(path.string().c_str(), RTLD_NOW);
    if (!handle) {
      throw std::runtime_error(dlerror());
    }
    ScopeGuard guard([&] { dlclose(handle); });

    auto alloc = reinterpret_cast<IPlugin* (*)()>(dlsym(handle, "allocator"));
    if (!alloc) {
      throw std::runtime_error("Symbol 'allocator' not found");
    }

    auto dealloc = reinterpret_cast<void (*)(IPlugin*)>(
        dlsym(handle, "deallocator"));
    if (!dealloc) {
      throw std::runtime_error("Symbol 'deallocator' not found");
    }
#endif

    IPlugin* raw = alloc();
    if (!raw) {
      throw std::runtime_error("allocator() returned nullptr");
    }

    PluginEntry entry;
    entry.path = path;
    entry.name = raw->name();
    entry.version = raw->version();
    entry.type = raw->type();

    dealloc(raw);

    return entry;
  }
};

}  // namespace plugin_arch
