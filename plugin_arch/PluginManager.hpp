// High-level plugin orchestrator.
//
// Composes PluginRegistry, PluginLoader, ServiceLocator and the opt-in mixins
// (IConfigurable, IServiceAware, ILifecycleAware, IDependencyAware) into a
// single component that handles:
//   - Discovery via PluginRegistry::scan()
//   - Dependency ordering via topological sort (Kahn's algorithm)
//   - Loading via PluginLoader<IPlugin>
//   - Wiring: configure() → set_service_locator() → on_init()
//   - Shutdown in reverse order
//
// Throws PluginError on load failures, std::runtime_error on cycles / missing deps.

#pragma once

#include <memory>
#include <queue>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "IConfigurable.hpp"
#include "IDependencyAware.hpp"
#include "ILifecycleAware.hpp"
#include "IPlugin.hpp"
#include "IServiceAware.hpp"
#include "PluginLoader.hpp"
#include "PluginRegistry.hpp"
#include "ServiceLocator.hpp"

namespace plugin_arch {

class PluginManager {
 private:
  struct PluginInfo {
    PluginEntry entry;
    std::vector<std::string> deps;
  };

 public:
  // Per-plugin configuration, keyed by plugin name.
  using ConfigMap = std::unordered_map<std::string, PluginConfig>;

  // Scan a directory and load all discovered plugins in dependency order.
  // config_map provides optional per-plugin configuration keyed by plugin name.
  void load_all(const std::filesystem::path& directory,
                const ConfigMap& config_map = {}) {
    // --- Discovery ---
    PluginRegistry registry;
    (void)registry.scan(directory);

    const auto& entries = registry.entries();
    if (entries.empty()) return;

    // --- Temporary load: probe each plugin for dependencies ---
    // We need to load each plugin briefly to check for IDependencyAware.
    // Build a map: type -> entry for resolution.
    std::vector<PluginInfo> infos;
    std::unordered_map<std::string, std::size_t> type_to_index;

    for (const auto& e : entries) {
      PluginInfo info;
      info.entry = e;

      // Probe for dependencies
      PluginLoader<IPlugin> probe_loader(e.path.string());
      auto probe_instance = probe_loader.get_instance();
      auto* dep_aware = dynamic_cast<IDependencyAware*>(probe_instance.get());
      if (dep_aware) {
        info.deps = dep_aware->dependencies();
      }

      type_to_index[e.type] = infos.size();
      infos.push_back(std::move(info));
    }

    // --- Topological sort (Kahn's algorithm) ---
    auto sorted = topological_sort(infos, type_to_index);

    // --- Load in order, wire mixins ---
    for (std::size_t idx : sorted) {
      const auto& info = infos[idx];
      const auto& entry = info.entry;

      PluginLoader<IPlugin> loader(entry.path.string());
      auto instance = loader.get_instance();

      // IConfigurable — configure before anything else
      auto* configurable = dynamic_cast<IConfigurable*>(instance.get());
      if (configurable) {
        if (auto it = config_map.find(entry.name); it != config_map.end()) {
          configurable->configure(it->second);
        }
      }

      // IServiceAware — inject the locator
      auto* aware = dynamic_cast<IServiceAware*>(instance.get());
      if (aware) {
        aware->set_service_locator(locator_);
      }

      // ILifecycleAware — call on_init()
      auto* lifecycle = dynamic_cast<ILifecycleAware*>(instance.get());
      if (lifecycle) {
        lifecycle->on_init();
      }

      // Register in the locator so downstream plugins can discover it
      locator_.add(instance);

      // Store for lifetime management and ordered shutdown
      loaders_.push_back(std::move(loader));
      instances_.push_back(std::move(instance));
    }
  }

  // Shut down all plugins in reverse load order.
  void shutdown() {
    for (auto it = instances_.rbegin(); it != instances_.rend(); ++it) {
      auto* lifecycle = dynamic_cast<ILifecycleAware*>(it->get());
      if (lifecycle) {
        lifecycle->on_shutdown();
      }
    }
    instances_.clear();
    loaders_.clear();
    locator_.clear();
  }

  [[nodiscard]] ServiceLocator& locator() { return locator_; }
  [[nodiscard]] const ServiceLocator& locator() const { return locator_; }

  [[nodiscard]] const std::vector<std::shared_ptr<IPlugin>>& instances() const {
    return instances_;
  }

 private:
  ServiceLocator locator_;
  std::vector<PluginLoader<IPlugin>> loaders_;
  std::vector<std::shared_ptr<IPlugin>> instances_;

  // Kahn's algorithm — returns indices into infos in topological order.
  // Throws on cycles or missing dependencies.
  static std::vector<std::size_t> topological_sort(
      const std::vector<PluginInfo>& infos,
      const std::unordered_map<std::string, std::size_t>& type_to_index) {
    const std::size_t n = infos.size();

    // Build adjacency list and in-degree count
    std::vector<std::vector<std::size_t>> adj(n);
    std::vector<std::size_t> in_degree(n, 0);

    for (std::size_t i = 0; i < n; ++i) {
      for (const auto& dep_type : infos[i].deps) {
        auto it = type_to_index.find(dep_type);
        if (it == type_to_index.end()) {
          throw std::runtime_error(
              "Plugin '" + infos[i].entry.name +
              "' depends on missing service type: " + dep_type);
        }
        std::size_t dep_idx = it->second;
        adj[dep_idx].push_back(i);  // dep_idx must come before i
        ++in_degree[i];
      }
    }

    // Seed queue with nodes that have no dependencies
    std::queue<std::size_t> ready;
    for (std::size_t i = 0; i < n; ++i) {
      if (in_degree[i] == 0) {
        ready.push(i);
      }
    }

    std::vector<std::size_t> order;
    order.reserve(n);

    while (!ready.empty()) {
      std::size_t cur = ready.front();
      ready.pop();
      order.push_back(cur);

      for (std::size_t next : adj[cur]) {
        if (--in_degree[next] == 0) {
          ready.push(next);
        }
      }
    }

    if (order.size() != n) {
      // Find plugins involved in the cycle for a useful error message
      std::string cycle_plugins;
      for (std::size_t i = 0; i < n; ++i) {
        if (in_degree[i] > 0) {
          if (!cycle_plugins.empty()) cycle_plugins += ", ";
          cycle_plugins += infos[i].entry.name;
        }
      }
      throw std::runtime_error("Circular dependency detected among: " +
                               cycle_plugins);
    }

    return order;
  }
};

}  // namespace plugin_arch
