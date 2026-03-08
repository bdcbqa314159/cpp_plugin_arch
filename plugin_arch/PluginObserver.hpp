// Observer interface for plugin lifecycle events.
//
// Hosts register observers with PluginManager to receive callbacks
// when plugins are loaded, unloaded, reloaded, enabled, or disabled.
// Useful for metrics, logging, and debugging.

#pragma once

#include <string>

namespace plugin_arch {

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

}  // namespace plugin_arch
