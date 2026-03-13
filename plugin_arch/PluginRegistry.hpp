// Plugin registry — scans a directory for shared libraries, probes each one
// for IPlugin metadata, and indexes the results. The host queries by type or
// name instead of relying on filename conventions.
//
// Discovers both typed plugins (allocator/deallocator) and C ABI plugins
// (plugin_describe). C ABI plugins are marked with is_dynamic = true.
//
// The registry indexes only. The host still uses PluginLoader<T> or
// DynamicPluginAdapter to do the actual loading when it wants to use a plugin.

#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "DynamicLibrary.hpp"
#include "IPlugin.hpp"
#include "IPluginMetadata.hpp"
#include "PluginDescriptor.hpp"
#include "PluginError.hpp"
#include "platform/shared_lib.hpp"

namespace plugin_arch {

// Owns all its strings — safe to use after the plugin library is unloaded.
struct PluginEntry {
  std::filesystem::path path;
  std::string name;
  std::string version;
  std::string type;

  // Extended metadata — populated when plugin implements IPluginMetadata.
  std::string author;
  std::string description;
  std::string license;
  std::vector<std::string> capabilities;

  // True for C ABI plugins discovered via plugin_describe().
  // False for typed plugins discovered via allocator/deallocator.
  bool is_dynamic = false;
};

struct ErrorRecord {
  std::filesystem::path path;
  std::string reason;
};

class PluginRegistry {
 public:
  PluginRegistry() = default;
  ~PluginRegistry() = default;

  PluginRegistry(const PluginRegistry&) = delete;
  PluginRegistry& operator=(const PluginRegistry&) = delete;
  PluginRegistry(PluginRegistry&&) = default;
  PluginRegistry& operator=(PluginRegistry&&) = default;

  // Scan a directory for shared libraries and probe each one for metadata.
  // Appends to existing entries, so you can scan multiple directories.
  // Returns the number of new plugins discovered in this call.
  [[nodiscard]] std::size_t scan(const std::filesystem::path& directory) {
    namespace fs = std::filesystem;

    if (!fs::is_directory(directory)) {
      throw LoadError::make(directory.string(), "not a directory");
    }

    std::size_t found = 0;

    for (const auto& entry : fs::directory_iterator(directory)) {
      std::error_code ec;
      if (!entry.is_regular_file(ec) || ec) continue;
      if (!platform::is_shared_library(entry.path())) continue;

      try {
        auto plugin_entry = probe(entry.path());
        entries_.push_back(std::move(plugin_entry));
        ++found;
      } catch (const std::exception&) {
        // Not a typed plugin — try C ABI
        try {
          auto dynamic_entry = probe_dynamic(entry.path());
          if (dynamic_entry) {
            entries_.push_back(std::move(*dynamic_entry));
            ++found;
          } else {
            errors_.push_back(
                {entry.path(),
                 "no allocator/deallocator or plugin_describe symbols"});
          }
        } catch (const std::exception& e2) {
          errors_.push_back({entry.path(), e2.what()});
        }
      }
    }

    return found;
  }

  // Get all plugins of a given type.
  [[nodiscard]] std::vector<PluginEntry> get_all(
      const std::string& type) const {
    std::vector<PluginEntry> result;
    for (const auto& e : entries_) {
      if (e.type == type) {
        result.push_back(e);
      }
    }
    return result;
  }

  // Get a plugin by name. Returns the first match.
  [[nodiscard]] std::optional<PluginEntry> get(const std::string& name) const {
    for (const auto& e : entries_) {
      if (e.name == name) {
        return e;
      }
    }
    return std::nullopt;
  }

  [[nodiscard]] const std::vector<PluginEntry>& entries() const {
    return entries_;
  }
  [[nodiscard]] const std::vector<ErrorRecord>& errors() const {
    return errors_;
  }

  void clear() {
    entries_.clear();
    errors_.clear();
  }

 private:
  std::vector<PluginEntry> entries_;
  std::vector<ErrorRecord> errors_;

  // Probe a typed plugin (allocator/deallocator → IPlugin metadata).
  static PluginEntry probe(const std::filesystem::path& path) {
    DynamicLibrary lib(path.string());

    auto alloc = lib.resolve<IPlugin* (*)()>("allocator");
    auto dealloc = lib.resolve<void (*)(IPlugin*)>("deallocator");

    IPlugin* raw = alloc();
    if (!raw) {
      throw AllocationError::make(path.string());
    }

    std::unique_ptr<IPlugin, decltype(dealloc)> guard(raw, dealloc);

    PluginEntry entry;
    entry.path = path;
    entry.name = raw->name();
    entry.version = raw->version();
    entry.type = raw->type();
    entry.is_dynamic = false;

    auto* meta = dynamic_cast<IPluginMetadata*>(raw);
    if (meta) {
      entry.author = meta->author();
      entry.description = meta->description();
      entry.license = meta->license();
      entry.capabilities = meta->capabilities();
    }

    return entry;
  }

  // Probe a C ABI plugin (plugin_describe → PluginDescriptor metadata).
  static std::optional<PluginEntry> probe_dynamic(
      const std::filesystem::path& path) {
    DynamicLibrary lib(path.string());

    if (!lib.has("plugin_describe")) return std::nullopt;

    auto describe =
        lib.resolve<const PluginDescriptor* (*)()>("plugin_describe");
    auto* desc = describe();
    if (!desc) return std::nullopt;

    PluginEntry entry;
    entry.path = path;
    entry.name = desc->name ? desc->name : "";
    entry.version = desc->version ? desc->version : "";
    entry.type = entry.name;  // C ABI plugins use name as type
    entry.is_dynamic = true;

    return entry;
  }
};

}  // namespace plugin_arch
