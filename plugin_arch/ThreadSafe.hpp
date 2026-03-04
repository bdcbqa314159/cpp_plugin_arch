// Thread-safe wrappers using std::shared_mutex (readers-writer lock).
//
// Zero cost for single-threaded hosts — they use the originals unchanged.
// Multi-threaded hosts wrap the component they need:
//
//   ThreadSafe<ServiceLocator> locator;
//   ThreadSafe<PluginRegistry> registry;
//   ThreadSafe<HotPluginLoader<T>> hot_loader;
//
// Read operations take a shared lock, write operations take a unique lock.

#pragma once

#include <memory>
#include <shared_mutex>
#include <string>
#include <vector>

#include "HotPluginLoader.hpp"
#include "PluginRegistry.hpp"
#include "ServiceLocator.hpp"

namespace plugin_arch {

// Primary template — not defined. Only the explicit specializations below
// are usable. Attempting ThreadSafe<SomeOtherType> gives a compile error.
template <typename T>
class ThreadSafe;

// --- ThreadSafe<ServiceLocator> ---

template <>
class ThreadSafe<ServiceLocator> {
 public:
  void add(const std::shared_ptr<IPlugin>& service) {
    std::unique_lock lock(mutex_);
    inner_.add(service);
  }

  template <typename T>
  [[nodiscard]] std::shared_ptr<T> get(const std::string& type) const {
    std::shared_lock lock(mutex_);
    return inner_.get<T>(type);
  }

  template <typename T>
  [[nodiscard]] std::vector<std::shared_ptr<T>> get_all(const std::string& type) const {
    std::shared_lock lock(mutex_);
    return inner_.get_all<T>(type);
  }

  void cleanup() {
    std::unique_lock lock(mutex_);
    inner_.cleanup();
  }

  void clear() {
    std::unique_lock lock(mutex_);
    inner_.clear();
  }

  [[nodiscard]] std::size_t size() const {
    std::shared_lock lock(mutex_);
    return inner_.size();
  }

 private:
  ServiceLocator inner_;
  mutable std::shared_mutex mutex_;
};

// --- ThreadSafe<PluginRegistry> ---

template <>
class ThreadSafe<PluginRegistry> {
 public:
  [[nodiscard]] std::size_t scan(const std::filesystem::path& directory) {
    std::unique_lock lock(mutex_);
    return inner_.scan(directory);
  }

  [[nodiscard]] std::vector<PluginEntry> get_all(const std::string& type) const {
    std::shared_lock lock(mutex_);
    return inner_.get_all(type);
  }

  [[nodiscard]] std::optional<PluginEntry> get(const std::string& name) const {
    std::shared_lock lock(mutex_);
    return inner_.get(name);
  }

  [[nodiscard]] std::vector<PluginEntry> entries() const {
    std::shared_lock lock(mutex_);
    return std::vector<PluginEntry>(inner_.entries().begin(), inner_.entries().end());
  }

  [[nodiscard]] std::vector<ErrorRecord> errors() const {
    std::shared_lock lock(mutex_);
    return std::vector<ErrorRecord>(inner_.errors().begin(), inner_.errors().end());
  }

  void clear() {
    std::unique_lock lock(mutex_);
    inner_.clear();
  }

 private:
  PluginRegistry inner_;
  mutable std::shared_mutex mutex_;
};

// --- ThreadSafe<HotPluginLoader<T>> ---

template <typename T>
class ThreadSafe<HotPluginLoader<T>> {
 public:
  explicit ThreadSafe(std::string library_path,
                      std::string alloc_symbol = "allocator",
                      std::string dealloc_symbol = "deallocator")
      : inner_(std::move(library_path), std::move(alloc_symbol),
               std::move(dealloc_symbol)) {}

  [[nodiscard]] std::shared_ptr<T> get_instance() const {
    std::shared_lock lock(mutex_);
    return inner_.get_instance();
  }

  [[nodiscard]] bool check_and_reload() {
    std::unique_lock lock(mutex_);
    return inner_.check_and_reload();
  }

  void reload() {
    std::unique_lock lock(mutex_);
    inner_.reload();
  }

  [[nodiscard]] std::string library_path() const {
    std::shared_lock lock(mutex_);
    return inner_.library_path();
  }

 private:
  HotPluginLoader<T> inner_;
  mutable std::shared_mutex mutex_;
};

}  // namespace plugin_arch
