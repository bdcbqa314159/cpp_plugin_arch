// Hot-reload wrapper around PluginLoader.
// Detects changes via std::filesystem::last_write_time() polling.
//
// Old plugin instances keep their library alive via shared_ptr in the
// deleter — no retirement mechanism needed. The host can safely hold
// shared_ptrs from previous versions indefinitely.

#pragma once

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

#include "ISerializable.hpp"
#include "PluginLoader.hpp"

namespace plugin_arch {

template <typename T>
class HotPluginLoader {
 public:
  explicit HotPluginLoader(std::string library_path,
                           std::string alloc_symbol = "allocator",
                           std::string dealloc_symbol = "deallocator")
      : library_path_(std::move(library_path)),
        alloc_symbol_(std::move(alloc_symbol)),
        dealloc_symbol_(std::move(dealloc_symbol)) {
    load();
  }

  HotPluginLoader(const HotPluginLoader&) = delete;
  HotPluginLoader& operator=(const HotPluginLoader&) = delete;

  HotPluginLoader(HotPluginLoader&&) noexcept = default;
  HotPluginLoader& operator=(HotPluginLoader&&) noexcept = default;

  [[nodiscard]] std::shared_ptr<T> get_instance() const { return instance_; }

  // Check file modification time; reload if changed. Returns true on reload.
  [[nodiscard]] bool check_and_reload() {
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
  // If the current instance implements ISerializable, state is preserved.
  void reload() {
    // Save state from old instance if it supports serialization
    std::string saved_state;
    bool has_state = false;
    if (instance_) {
      auto* serializable = dynamic_cast<ISerializable*>(instance_.get());
      if (serializable) {
        saved_state = serializable->save_state();
        has_state = true;
      }
    }

    // Create the new loader first — if this throws, current state is untouched.
    auto new_loader = std::make_unique<PluginLoader<T>>(
        library_path_, alloc_symbol_, dealloc_symbol_);
    auto new_instance = new_loader->get_instance();

    // Restore state into the new instance
    if (has_state) {
      auto* serializable = dynamic_cast<ISerializable*>(new_instance.get());
      if (serializable) {
        serializable->restore_state(saved_state);
      }
    }

    // Success — swap in the new version.
    // Old instances (if any) keep the old library alive via their deleters.
    loader_ = std::move(new_loader);
    instance_ = std::move(new_instance);

    std::error_code ec;
    last_modified_ = std::filesystem::last_write_time(library_path_, ec);
  }

  [[nodiscard]] const std::string& library_path() const { return library_path_; }

 private:
  std::string library_path_;
  std::string alloc_symbol_;
  std::string dealloc_symbol_;
  std::filesystem::file_time_type last_modified_;

  std::unique_ptr<PluginLoader<T>> loader_;
  std::shared_ptr<T> instance_;

  void load() {
    std::error_code ec;
    last_modified_ = std::filesystem::last_write_time(library_path_, ec);
    if (ec) {
      throw std::runtime_error("Cannot stat library: " + library_path_);
    }

    loader_ = std::make_unique<PluginLoader<T>>(library_path_, alloc_symbol_,
                                                dealloc_symbol_);
    instance_ = loader_->get_instance();
  }
};

}  // namespace plugin_arch
