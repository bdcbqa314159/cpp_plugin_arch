// Observer interface for plugin lifecycle events.
//
// Hosts register observers with PluginManager to receive callbacks
// when plugins are loaded, unloaded, reloaded, enabled, or disabled.
// Useful for metrics, logging, and debugging.
//
// Use ObserverGuard for RAII auto-deregistration to prevent dangling pointers.

#pragma once

#include <string>

namespace plugin_arch {

class PluginManager;

class PluginObserver {
 public:
  virtual ~PluginObserver() = default;

  virtual void on_plugin_loaded(const std::string& name,
                                const std::string& type) {}
  virtual void on_plugin_unloaded(const std::string& name,
                                  const std::string& type) {}
  virtual void on_plugin_reloaded(const std::string& name,
                                  const std::string& type) {}
  virtual void on_plugin_enabled(const std::string& name,
                                 const std::string& type) {}
  virtual void on_plugin_disabled(const std::string& name,
                                  const std::string& type) {}
};

// RAII guard that auto-deregisters the observer on destruction.
// Prevents dangling pointer callbacks when the observer is destroyed
// before the PluginManager.
class ObserverGuard {
 public:
  ObserverGuard(PluginManager& manager, PluginObserver* observer);
  ~ObserverGuard();

  ObserverGuard(const ObserverGuard&) = delete;
  ObserverGuard& operator=(const ObserverGuard&) = delete;

  ObserverGuard(ObserverGuard&& other) noexcept
      : manager_(other.manager_), observer_(other.observer_) {
    other.observer_ = nullptr;
  }

  ObserverGuard& operator=(ObserverGuard&& other) noexcept {
    if (this != &other) {
      release();
      manager_ = other.manager_;
      observer_ = other.observer_;
      other.observer_ = nullptr;
    }
    return *this;
  }

  void release();

 private:
  PluginManager* manager_;
  PluginObserver* observer_;
};

}  // namespace plugin_arch
