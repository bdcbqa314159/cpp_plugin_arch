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

#include <exception>
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

// Plugin metadata + dependency list — used for discovery and topological sort.
// Public so tests and hosts can construct instances for PluginManager::add_plugin().
struct PluginInfo {
  PluginEntry entry;
  std::vector<std::string> deps;
};

// Kahn's algorithm — returns indices grouped by topological level.
// Plugins within a level have no dependencies on each other.
// Throws on cycles or missing dependencies.
// Free function so it can be tested independently.
inline std::vector<std::vector<std::size_t>> topological_sort_levels(
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

class PluginManager {
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

    auto [infos, levels] = discover_and_sort(directory, policy, &load_errors_);

    // Track failed types so dependents are skipped in best_effort mode.
    std::unordered_set<std::string> failed_types;

    for (const auto& level : levels) {
      for (std::size_t idx : level) {
        const auto& info = infos[idx];

        if (policy == LoadPolicy::strict) {
          load_and_wire(info.entry, config_map);
          continue;
        }

        if (has_failed_dep(info, failed_types)) {
          record_cascade_skip(info, failed_types);
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

  // Same as load_all() but parallelizes plugin loading within each
  // topological level. Plugins at the same level have no dependencies on
  // each other and can be loaded concurrently. The call itself blocks
  // until all plugins are loaded.
  void load_all_parallel(const std::filesystem::path& directory,
                         const ConfigMap& config_map = {},
                         LoadPolicy policy = LoadPolicy::strict) {
    load_errors_.clear();

    auto [infos, levels] = discover_and_sort(directory, policy, &load_errors_);

    std::unordered_set<std::string> failed_types;

    for (const auto& level : levels) {
      // Filter out plugins with failed dependencies (best_effort).
      std::vector<std::size_t> loadable;
      for (std::size_t idx : level) {
        if (policy == LoadPolicy::best_effort &&
            has_failed_dep(infos[idx], failed_types)) {
          record_cascade_skip(infos[idx], failed_types);
          continue;
        }
        loadable.push_back(idx);
      }

      if (loadable.size() <= 1) {
        // Single plugin (or empty after filtering) — no async overhead.
        for (std::size_t idx : loadable) {
          try_load_and_wire(infos[idx].entry, config_map, policy, failed_types);
        }
        continue;
      }

      // Parallel load: dlopen + get_instance on separate threads.
      struct LoadResult {
        std::optional<PluginLoader<IPlugin>> loader;
        std::shared_ptr<IPlugin> instance;
        std::size_t info_idx;
        std::exception_ptr error;
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
              } catch (...) {
                return {std::nullopt, nullptr, idx, std::current_exception()};
              }
            }));
      }

      // Collect results and wire sequentially.
      for (auto& fut : futures) {
        auto result = fut.get();
        if (result.error) {
          if (policy == LoadPolicy::best_effort) {
            failed_types.insert(infos[result.info_idx].entry.type);
            try {
              std::rethrow_exception(result.error);
            } catch (const std::exception& e) {
              load_errors_.push_back(
                  {infos[result.info_idx].entry.path, e.what()});
            }
            continue;
          }
          std::rethrow_exception(result.error);
        }
        wire_instance(result.instance, infos[result.info_idx].entry, config_map);
        locator_.add(result.instance);
        loaders_.push_back(std::move(*result.loader));
        instances_.push_back(std::move(result.instance));
      }
    }
  }

  // Register a pre-loaded plugin instance. Useful for testing without
  // the filesystem — construct your plugin, wire it through the manager.
  void add_plugin(std::shared_ptr<IPlugin> instance,
                  const PluginEntry& entry,
                  const ConfigMap& config_map = {}) {
    wire_instance(instance, entry, config_map);
    locator_.add(instance);
    instances_.push_back(std::move(instance));
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
  [[nodiscard]] const std::vector<ErrorRecord>& load_errors() const {
    return load_errors_;
  }

 private:
  ServiceLocator locator_;
  EventBus event_bus_;
  std::vector<PluginLoader<IPlugin>> loaders_;
  std::vector<std::shared_ptr<IPlugin>> instances_;
  std::vector<ErrorRecord> load_errors_;

  // --- Discovery ---

  // Scan directory, probe each plugin for dependencies, topologically sort.
  struct DiscoveryResult {
    std::vector<PluginInfo> infos;
    std::vector<std::vector<std::size_t>> levels;
  };

  static DiscoveryResult discover_and_sort(
      const std::filesystem::path& directory,
      LoadPolicy policy = LoadPolicy::strict,
      std::vector<ErrorRecord>* errors_out = nullptr) {
    PluginRegistry registry;
    (void)registry.scan(directory);

    const auto& entries = registry.entries();
    if (entries.empty()) return {};

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

      auto [it, inserted] = type_to_index.try_emplace(e.type, infos.size());
      if (!inserted) {
        if (policy == LoadPolicy::strict) {
          throw std::runtime_error(
              "Duplicate plugin type '" + e.type + "': '" +
              infos[it->second].entry.name + "' and '" + e.name + "'");
        }
        // best_effort: skip duplicate, record error
        if (errors_out) {
          errors_out->push_back(
              {e.path, "duplicate plugin type '" + e.type +
                       "', already provided by '" +
                       infos[it->second].entry.name + "'"});
        }
        continue;
      }
      infos.push_back(std::move(info));
    }

    auto levels = topological_sort_levels(infos, type_to_index);
    return {std::move(infos), std::move(levels)};
  }

  // --- Dependency cascade ---

  static bool has_failed_dep(const PluginInfo& info,
                             const std::unordered_set<std::string>& failed_types) {
    for (const auto& dep : info.deps) {
      if (failed_types.count(dep)) return true;
    }
    return false;
  }

  void record_cascade_skip(const PluginInfo& info,
                           std::unordered_set<std::string>& failed_types) {
    failed_types.insert(info.entry.type);
    load_errors_.push_back(
        {info.entry.path, "skipped: dependency unavailable"});
  }

  // --- Loading + wiring ---

  // Load a single plugin with policy-aware error handling.
  void try_load_and_wire(const PluginEntry& entry,
                         const ConfigMap& config_map,
                         LoadPolicy policy,
                         std::unordered_set<std::string>& failed_types) {
    if (policy == LoadPolicy::strict) {
      load_and_wire(entry, config_map);
    } else {
      try {
        load_and_wire(entry, config_map);
      } catch (const std::exception& e) {
        failed_types.insert(entry.type);
        load_errors_.push_back({entry.path, e.what()});
      }
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

  // Wire all opt-in mixins on an instance.
  // Order matters: configure → inject services → inject events → init.
  // To add a new mixin: add a dynamic_cast + call block here, before on_init().
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

    // on_init() must be last — plugin may use injected services/events.
    auto* lifecycle = dynamic_cast<ILifecycleAware*>(instance.get());
    if (lifecycle) {
      lifecycle->on_init();
    }
  }
};

}  // namespace plugin_arch
