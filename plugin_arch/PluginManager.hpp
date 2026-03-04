// High-level plugin orchestrator.
//
// Composes PluginRegistry, PluginLoader, ServiceLocator, EventBus and the
// opt-in mixins (IConfigurable, IServiceAware, IEventAware, ILifecycleAware,
// IDependencyAware) into a single component that handles:
//   - Discovery via PluginRegistry::scan()
//   - Dependency ordering via topological sort (Kahn's algorithm)
//   - Loading via PluginLoader<IPlugin>
//   - Wiring: configure() → set_service_locator() → set_event_bus() → on_init()
//   - Shutdown in reverse order
//
// Throws PluginError on load failures, std::runtime_error on cycles / missing deps.

#pragma once

#include <future>
#include <memory>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "EventBus.hpp"
#include "IConfigurable.hpp"
#include "IDependencyAware.hpp"
#include "IEventAware.hpp"
#include "ILifecycleAware.hpp"
#include "IPlugin.hpp"
#include "IServiceAware.hpp"
#include "PluginLoader.hpp"
#include "PluginRegistry.hpp"
#include "ServiceLocator.hpp"

namespace plugin_arch {

enum class LoadPolicy {
  strict,       // throw on first failure (default)
  best_effort   // skip failed plugins, record errors
};

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
  // policy: strict (throw on first failure) or best_effort (skip, record errors).
  void load_all(const std::filesystem::path& directory,
                const ConfigMap& config_map = {},
                LoadPolicy policy = LoadPolicy::strict) {
    load_errors_.clear();

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
    auto levels = topological_sort_levels(infos, type_to_index);

    // --- Load level by level, wire mixins ---
    // In best_effort mode, track failed types so dependents are skipped.
    std::unordered_set<std::string> failed_types;

    for (const auto& level : levels) {
      for (std::size_t idx : level) {
        const auto& info = infos[idx];

        if (policy == LoadPolicy::strict) {
          load_and_wire(info.entry, config_map);
        } else {
          // Skip if any dependency failed
          bool dep_failed = false;
          for (const auto& dep : info.deps) {
            if (failed_types.count(dep)) {
              dep_failed = true;
              break;
            }
          }
          if (dep_failed) {
            failed_types.insert(info.entry.type);
            load_errors_.push_back(
                {info.entry.path,
                 "skipped: dependency unavailable"});
            continue;
          }

          try {
            load_and_wire(info.entry, config_map);
          } catch (const std::exception& e) {
            failed_types.insert(info.entry.type);
            load_errors_.push_back({info.entry.path, e.what()});
          }
        }
      }
    }
  }

  // Same as load_all() but parallelizes plugin loading within each
  // topological level. Plugins at the same level have no dependencies on
  // each other and can be loaded concurrently.
  void load_all_async(const std::filesystem::path& directory,
                      const ConfigMap& config_map = {},
                      LoadPolicy policy = LoadPolicy::strict) {
    load_errors_.clear();

    PluginRegistry registry;
    (void)registry.scan(directory);

    const auto& entries = registry.entries();
    if (entries.empty()) return;

    std::vector<PluginInfo> infos;
    std::unordered_map<std::string, std::size_t> type_to_index;

    for (const auto& e : entries) {
      PluginInfo info;
      info.entry = e;

      PluginLoader<IPlugin> probe_loader(e.path.string());
      auto probe_instance = probe_loader.get_instance();
      auto* dep_aware = dynamic_cast<IDependencyAware*>(probe_instance.get());
      if (dep_aware) {
        info.deps = dep_aware->dependencies();
      }

      type_to_index[e.type] = infos.size();
      infos.push_back(std::move(info));
    }

    auto levels = topological_sort_levels(infos, type_to_index);

    // Track failed types for best_effort dependency cascade.
    std::unordered_set<std::string> failed_types;

    // Load each level: plugins within a level are independent and can
    // be loaded in parallel. Wiring (mixin injection) is sequential
    // since it touches shared state (locator_, event_bus_).
    for (const auto& level : levels) {
      if (level.size() == 1) {
        const auto& info = infos[level[0]];

        if (policy == LoadPolicy::best_effort) {
          bool dep_failed = false;
          for (const auto& dep : info.deps) {
            if (failed_types.count(dep)) { dep_failed = true; break; }
          }
          if (dep_failed) {
            failed_types.insert(info.entry.type);
            load_errors_.push_back(
                {info.entry.path, "skipped: dependency unavailable"});
            continue;
          }
          try {
            load_and_wire(info.entry, config_map);
          } catch (const std::exception& e) {
            failed_types.insert(info.entry.type);
            load_errors_.push_back({info.entry.path, e.what()});
          }
        } else {
          load_and_wire(info.entry, config_map);
        }
        continue;
      }

      // Filter out plugins with failed dependencies (best_effort)
      std::vector<std::size_t> loadable;
      for (std::size_t idx : level) {
        const auto& info = infos[idx];
        if (policy == LoadPolicy::best_effort) {
          bool dep_failed = false;
          for (const auto& dep : info.deps) {
            if (failed_types.count(dep)) { dep_failed = true; break; }
          }
          if (dep_failed) {
            failed_types.insert(info.entry.type);
            load_errors_.push_back(
                {info.entry.path, "skipped: dependency unavailable"});
            continue;
          }
        }
        loadable.push_back(idx);
      }

      // Parallel load: dlopen + get_instance on separate threads
      struct LoadResult {
        std::optional<PluginLoader<IPlugin>> loader;
        std::shared_ptr<IPlugin> instance;
        std::size_t info_idx;
        std::string error;
      };

      std::vector<std::future<LoadResult>> futures;
      futures.reserve(loadable.size());

      for (std::size_t idx : loadable) {
        const auto& entry = infos[idx].entry;
        futures.push_back(std::async(std::launch::async,
            [&entry, idx]() -> LoadResult {
              try {
                PluginLoader<IPlugin> loader(entry.path.string());
                auto instance = loader.get_instance();
                return {std::move(loader), std::move(instance), idx, {}};
              } catch (const std::exception& e) {
                return {std::nullopt, nullptr, idx, e.what()};
              }
            }));
      }

      // Collect results and wire sequentially
      for (auto& fut : futures) {
        auto result = fut.get();
        if (!result.error.empty()) {
          if (policy == LoadPolicy::best_effort) {
            failed_types.insert(infos[result.info_idx].entry.type);
            load_errors_.push_back(
                {infos[result.info_idx].entry.path, result.error});
            continue;
          }
          throw std::runtime_error(result.error);
        }
        wire_instance(result.instance, infos[result.info_idx].entry, config_map);
        locator_.add(result.instance);
        loaders_.push_back(std::move(*result.loader));
        instances_.push_back(std::move(result.instance));
      }
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
    event_bus_.clear();
    load_errors_.clear();
  }

  [[nodiscard]] ServiceLocator& locator() { return locator_; }
  [[nodiscard]] const ServiceLocator& locator() const { return locator_; }

  [[nodiscard]] EventBus& event_bus() { return event_bus_; }
  [[nodiscard]] const EventBus& event_bus() const { return event_bus_; }

  [[nodiscard]] const std::vector<std::shared_ptr<IPlugin>>& instances() const {
    return instances_;
  }

  // Errors from best_effort loading (empty in strict mode).
  [[nodiscard]] const std::vector<ScanError>& load_errors() const {
    return load_errors_;
  }

 private:
  ServiceLocator locator_;
  EventBus event_bus_;
  std::vector<PluginLoader<IPlugin>> loaders_;
  std::vector<std::shared_ptr<IPlugin>> instances_;
  std::vector<ScanError> load_errors_;

  // Wire all opt-in mixins on an instance.
  void wire_instance(std::shared_ptr<IPlugin>& instance,
                     const PluginEntry& entry,
                     const ConfigMap& config_map) {
    auto* configurable = dynamic_cast<IConfigurable*>(instance.get());
    if (configurable) {
      if (auto it = config_map.find(entry.name); it != config_map.end()) {
        configurable->configure(it->second);
      }
    }

    auto* aware = dynamic_cast<IServiceAware*>(instance.get());
    if (aware) {
      aware->set_service_locator(locator_);
    }

    auto* event_aware = dynamic_cast<IEventAware*>(instance.get());
    if (event_aware) {
      event_aware->set_event_bus(event_bus_);
    }

    auto* lifecycle = dynamic_cast<ILifecycleAware*>(instance.get());
    if (lifecycle) {
      lifecycle->on_init();
    }
  }

  // Load a single plugin and wire it.
  void load_and_wire(const PluginEntry& entry, const ConfigMap& config_map) {
    PluginLoader<IPlugin> loader(entry.path.string());
    auto instance = loader.get_instance();
    wire_instance(instance, entry, config_map);
    locator_.add(instance);
    loaders_.push_back(std::move(loader));
    instances_.push_back(std::move(instance));
  }

  // Kahn's algorithm — returns indices grouped by topological level.
  // Plugins within a level have no dependencies on each other.
  // Throws on cycles or missing dependencies.
  static std::vector<std::vector<std::size_t>> topological_sort_levels(
      const std::vector<PluginInfo>& infos,
      const std::unordered_map<std::string, std::size_t>& type_to_index) {
    const std::size_t n = infos.size();

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
        adj[dep_idx].push_back(i);
        ++in_degree[i];
      }
    }

    std::queue<std::size_t> ready;
    for (std::size_t i = 0; i < n; ++i) {
      if (in_degree[i] == 0) {
        ready.push(i);
      }
    }

    std::vector<std::vector<std::size_t>> levels;
    std::size_t processed = 0;

    while (!ready.empty()) {
      // Drain all currently ready nodes — they form one level
      std::vector<std::size_t> level;
      std::size_t level_size = ready.size();
      for (std::size_t i = 0; i < level_size; ++i) {
        std::size_t cur = ready.front();
        ready.pop();
        level.push_back(cur);
        ++processed;

        for (std::size_t next : adj[cur]) {
          if (--in_degree[next] == 0) {
            ready.push(next);
          }
        }
      }
      levels.push_back(std::move(level));
    }

    if (processed != n) {
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

    return levels;
  }
};

}  // namespace plugin_arch
