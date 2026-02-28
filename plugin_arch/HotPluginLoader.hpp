// Hot-reload wrapper around PluginLoader.
// Detects changes via std::filesystem::last_write_time() polling.
// On reload, the old PluginLoader is kept alive until all shared_ptrs
// to the old instance are released, preventing the deleter-after-dlclose crash.

#pragma once

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "PluginLoader.hpp"

namespace plugin_arch {

template <typename T>
class HotPluginLoader {
 public:
  explicit HotPluginLoader(const std::string& library_path,
                           const std::string& alloc_symbol = "allocator",
                           const std::string& dealloc_symbol = "deallocator")
      : library_path_(library_path),
        alloc_symbol_(alloc_symbol),
        dealloc_symbol_(dealloc_symbol) {
    load();
  }

  ~HotPluginLoader() {
    instance_.reset();
    cleanup_retired();
  }

  HotPluginLoader(const HotPluginLoader&) = delete;
  HotPluginLoader& operator=(const HotPluginLoader&) = delete;

  std::shared_ptr<T> get_instance() { return instance_; }

  // Check file modification time; reload if changed. Returns true on reload.
  bool check_and_reload() {
    std::error_code ec;
    auto mtime = std::filesystem::last_write_time(library_path_, ec);
    if (ec) return false;

    if (mtime != last_modified_) {
      try {
        reload();
        return true;
      } catch (const std::exception& e) {
        std::cerr << "[HotPluginLoader] reload failed: " << e.what()
                  << " — keeping current version\n";
        last_modified_ = mtime;  // avoid retrying every poll
        return false;
      }
    }
    return false;
  }

  // Force reload regardless of modification time.
  void reload() {
    // Retire current loader + weak reference to its instance
    retired_.push_back({std::move(loader_), instance_});
    instance_.reset();

    load();
    cleanup_retired();
  }

  const std::string& library_path() const { return library_path_; }

 private:
  std::string library_path_;
  std::string alloc_symbol_;
  std::string dealloc_symbol_;
  std::filesystem::file_time_type last_modified_;

  std::unique_ptr<PluginLoader<T>> loader_;
  std::shared_ptr<T> instance_;

  struct RetiredPlugin {
    std::unique_ptr<PluginLoader<T>> loader;
    std::weak_ptr<T> instance;
  };
  std::vector<RetiredPlugin> retired_;

  void load() {
    std::error_code ec;
    last_modified_ = std::filesystem::last_write_time(library_path_, ec);
    if (ec) {
      throw std::runtime_error("Cannot stat library: " + library_path_);
    }

    loader_ =
        std::make_unique<PluginLoader<T>>(library_path_, alloc_symbol_, dealloc_symbol_);
    instance_ = loader_->get_instance();
  }

  // Erase retired entries whose shared_ptrs have all been released.
  void cleanup_retired() {
    retired_.erase(
        std::remove_if(retired_.begin(), retired_.end(),
                       [](const RetiredPlugin& r) { return r.instance.expired(); }),
        retired_.end());
  }
};

}  // namespace plugin_arch
