// High-level plugin orchestrator.
//
// Composes PluginRegistry, PluginLoader, ServiceLocator, EventBus,
// DynamicPluginAdapter, and the opt-in mixins into a single component
// that handles:
//   - Discovery via PluginRegistry::scan() (typed + C ABI plugins)
//   - Dependency ordering via topological sort (Kahn's algorithm)
//   - Version constraint checking (SemVer)
//   - Loading via PluginLoader<IPlugin> or DynamicPluginAdapter
//   - Wiring: configure() → set_service_locator() → set_event_bus() → on_init()
//   - Per-plugin unload with reverse-dependency cascade
//   - Per-plugin reload with state preservation
//   - Shutdown in reverse order
//
// Throws PluginError on load failures, std::runtime_error on cycles / missing deps.

#pragma once

#include <algorithm>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "DynamicPluginAdapter.hpp"
#include "EventBus.hpp"
#include "IConfigSchema.hpp"
#include "IConfigurable.hpp"
#include "IConflictAware.hpp"
#include "IDependencyAware.hpp"
#include "IEventAware.hpp"
#include "IHealthAware.hpp"
#include "ILifecycleAware.hpp"
#include "IPlugin.hpp"
#include "IPluginMetadata.hpp"
#include "ISerializable.hpp"
#include "IServiceAware.hpp"
#include "PluginLoader.hpp"
#include "PluginObserver.hpp"
#include "PluginRegistry.hpp"
#include "SemVer.hpp"
#include "ServiceLocator.hpp"

namespace plugin_arch {

enum class LoadPolicy {
  strict,      // throw on first failure (default)
  best_effort  // skip failed plugins, record errors
};

// Plugin metadata + dependency list — used for discovery and topological sort.
// Public so tests and hosts can construct instances for PluginManager::add_plugin().
struct DiscoveredPlugin {
  PluginEntry entry;
  std::vector<std::string> deps;       // type strings (version stripped, for topo sort)
  std::vector<std::string> version_constraints;   // original strings (for version checking)
  std::vector<std::string> conflicts;  // mutually exclusive type strings
};

// Kahn's algorithm — returns indices grouped by topological level.
// Plugins within a level have no dependencies on each other.
// Throws on cycles or missing dependencies.
// Free function so it can be tested independently.
inline std::vector<std::vector<std::size_t>> topological_sort_levels(
    const std::vector<DiscoveredPlugin>& infos,
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
    // Find an actual cycle path via DFS for a clear error message.
    std::string cycle_chain;

    // Build adjacency from deps for DFS (forward edges: plugin → its dependency)
    std::vector<std::vector<std::size_t>> fwd(n);
    for (std::size_t i = 0; i < n; ++i) {
      if (in_degree[i] == 0) continue;  // not in cycle
      for (const auto& dep_type : infos[i].deps) {
        auto dit = type_to_index.find(dep_type);
        if (dit != type_to_index.end() && in_degree[dit->second] > 0) {
          fwd[i].push_back(dit->second);
        }
      }
    }

    // DFS from first unprocessed node to find a cycle
    enum class Color { white, gray, black };
    std::vector<Color> color(n, Color::white);
    std::vector<std::size_t> parent(n, n);

    std::function<bool(std::size_t)> dfs = [&](std::size_t u) -> bool {
      color[u] = Color::gray;
      for (std::size_t v : fwd[u]) {
        if (color[v] == Color::gray) {
          // Found cycle: trace back from u to v
          cycle_chain = infos[v].entry.name;
          std::size_t cur = u;
          std::vector<std::string> path;
          while (cur != v) {
            path.push_back(infos[cur].entry.name);
            cur = parent[cur];
          }
          for (auto rit = path.rbegin(); rit != path.rend(); ++rit) {
            cycle_chain += " -> " + *rit;
          }
          cycle_chain += " -> " + infos[v].entry.name;
          return true;
        }
        if (color[v] == Color::white) {
          parent[v] = u;
          if (dfs(v)) return true;
        }
      }
      color[u] = Color::black;
      return false;
    };

    for (std::size_t i = 0; i < n; ++i) {
      if (in_degree[i] > 0 && color[i] == Color::white) {
        if (dfs(i)) break;
      }
    }

    if (cycle_chain.empty()) {
      // Fallback: list all involved plugins
      for (std::size_t i = 0; i < n; ++i) {
        if (in_degree[i] > 0) {
          if (!cycle_chain.empty()) cycle_chain += ", ";
          cycle_chain += infos[i].entry.name;
        }
      }
    }

    throw std::runtime_error("Circular dependency detected: " +
                             cycle_chain);
  }

  return levels;
}

// Check version constraints between discovered plugins.
// Throws in strict mode, records errors in best_effort mode.
inline void check_version_constraints(
    const std::vector<DiscoveredPlugin>& infos,
    const std::unordered_map<std::string, std::size_t>& type_to_index,
    LoadPolicy policy,
    std::vector<ErrorRecord>* errors_out) {
  for (const auto& info : infos) {
    for (const auto& raw_dep : info.version_constraints) {
      try {
        auto parsed = Dependency::parse(raw_dep);
        if (parsed.op == Dependency::Op::any) continue;

        auto provider_it = type_to_index.find(parsed.type);
        if (provider_it == type_to_index.end()) continue;  // topo sort will catch

        const auto& provider = infos[provider_it->second];
        auto provider_version = SemVer::parse(provider.entry.version);

        if (!parsed.satisfied_by(provider_version)) {
          std::string msg = "Plugin '" + info.entry.name +
                            "' requires " + raw_dep + " but '" +
                            provider.entry.name + "' provides version " +
                            provider.entry.version;
          if (policy == LoadPolicy::strict) {
            throw std::runtime_error(msg);
          }
          if (errors_out) {
            errors_out->push_back({info.entry.path, msg});
          }
        }
      } catch (const std::runtime_error& e) {
        if (policy == LoadPolicy::strict) throw;
        if (errors_out) {
          errors_out->push_back({info.entry.path, e.what()});
        }
      }
    }
  }
}

// Check mutual exclusion constraints between discovered plugins.
// Throws in strict mode, records errors in best_effort mode.
inline void check_conflict_constraints(
    const std::vector<DiscoveredPlugin>& infos,
    const std::unordered_map<std::string, std::size_t>& type_to_index,
    LoadPolicy policy,
    std::vector<ErrorRecord>* errors_out) {
  for (const auto& info : infos) {
    for (const auto& conflict_type : info.conflicts) {
      auto conflict_it = type_to_index.find(conflict_type);
      if (conflict_it == type_to_index.end()) continue;

      std::string msg = "Plugin '" + info.entry.name +
                        "' conflicts with '" +
                        infos[conflict_it->second].entry.name +
                        "' (type: " + conflict_type + ")";
      if (policy == LoadPolicy::strict) {
        throw std::runtime_error(msg);
      }
      if (errors_out) {
        errors_out->push_back({info.entry.path, msg});
      }
    }
  }
}

// Validate a config map against a plugin's IConfigSchema.
// Pure query — does not mutate the config.
// Returns a list of error messages (empty = valid).
inline std::vector<std::string> validate_plugin_config(
    IPlugin* instance, const PluginConfig& config) {
  if (!instance) return {};
  auto* schema_aware = dynamic_cast<IConfigSchema*>(instance);
  if (!schema_aware) return {};  // no schema = always valid

  std::vector<std::string> errors;
  auto schema = schema_aware->config_schema();

  std::unordered_set<std::string> known_keys;
  for (const auto& key_def : schema) {
    known_keys.insert(key_def.key);
    if (config.find(key_def.key) == config.end()) {
      if (key_def.required) {
        errors.push_back("Missing required config key: " + key_def.key);
      }
    }
  }

  for (const auto& [key, value] : config) {
    if (!known_keys.contains(key)) {
      errors.push_back("Unknown config key: " + key);
    }
  }

  return errors;
}

// Apply default values from a plugin's IConfigSchema to a config map.
// Inserts defaults for optional keys that are absent.
inline void apply_config_defaults(IPlugin* instance, PluginConfig& config) {
  if (!instance) return;
  auto* schema_aware = dynamic_cast<IConfigSchema*>(instance);
  if (!schema_aware) return;

  for (const auto& key_def : schema_aware->config_schema()) {
    if (config.find(key_def.key) == config.end() &&
        !key_def.required && !key_def.default_value.empty()) {
      config[key_def.key] = key_def.default_value;
    }
  }
}

class PluginManager {
 public:
  PluginManager() = default;
  virtual ~PluginManager() = default;

  PluginManager(const PluginManager&) = delete;
  PluginManager& operator=(const PluginManager&) = delete;
  PluginManager(PluginManager&&) = delete;
  PluginManager& operator=(PluginManager&&) = delete;

  // Per-plugin configuration, keyed by plugin name.
  using ConfigMap = std::unordered_map<std::string, PluginConfig>;

  // Per-plugin health report.
  struct PluginHealthReport {
    std::string plugin_name;
    HealthStatus status;
  };

  // State for a loaded plugin — tracks everything needed for unload/reload.
  struct LoadedPlugin {
    std::shared_ptr<IPlugin> instance;
    std::optional<PluginLoader<IPlugin>> loader;  // nullopt for add_plugin()
                                                  // and C ABI adapters
    PluginEntry entry;
    std::vector<std::string> deps;  // dependency type strings
    bool enabled = true;
    std::vector<EventBus::SubscriptionId> subscription_ids;  // tracked for disable/unsubscribe
  };

  // Scan a directory and load all discovered plugins in dependency order.
  // Supports both typed (IPlugin) and C ABI (PluginDescriptor) plugins.
  // config_map provides optional per-plugin configuration keyed by plugin name.
  // policy: strict (throw on first failure) or best_effort (skip, record errors).
  void load_all(const std::filesystem::path& directory,
                const ConfigMap& config_map = {},
                LoadPolicy policy = LoadPolicy::strict) {
    PluginRegistry registry;
    (void)registry.scan(directory);
    load_all(registry, config_map, policy);
  }

  // Load from a pre-populated registry. Allows hosts to use custom discovery
  // (e.g., database-backed, network, or filtered) instead of directory scanning.
  void load_all(PluginRegistry& registry,
                const ConfigMap& config_map = {},
                LoadPolicy policy = LoadPolicy::strict) {
    load_errors_.clear();

    auto [infos, levels] = discover_and_sort(registry, policy, &load_errors_);

    std::unordered_set<std::string> failed_types;

    for (const auto& level : levels) {
      for (std::size_t idx : level) {
        const auto& info = infos[idx];

        if (policy == LoadPolicy::strict) {
          load_and_wire(info.entry, info.deps, config_map);
          continue;
        }

        if (has_failed_dep(info, failed_types)) {
          record_cascade_skip(info, failed_types);
          continue;
        }

        try {
          load_and_wire(info.entry, info.deps, config_map);
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
    PluginRegistry registry;
    (void)registry.scan(directory);
    load_errors_.clear();

    auto [infos, levels] = discover_and_sort(registry, policy, &load_errors_);

    std::unordered_set<std::string> failed_types;

    for (const auto& level : levels) {
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
        for (std::size_t idx : loadable) {
          try_load_and_wire(infos[idx].entry, infos[idx].deps, config_map,
                            policy, failed_types);
        }
        continue;
      }

      // Parallel load: dlopen + get_instance on separate threads.
      struct LoadResult {
        std::shared_ptr<IPlugin> instance;
        std::optional<PluginLoader<IPlugin>> loader;
        std::size_t info_idx;
        std::exception_ptr error;
      };

      std::vector<std::future<LoadResult>> futures;
      futures.reserve(loadable.size());

      for (std::size_t idx : loadable) {
        const auto& entry = infos[idx].entry;
        futures.push_back(
            std::async(std::launch::async, [&entry, idx]() -> LoadResult {
              try {
                if (entry.is_dynamic) {
                  auto adapter =
                      DynamicPluginAdapter::load(entry.path.string());
                  return {std::move(adapter), std::nullopt, idx, {}};
                }
                PluginLoader<IPlugin> loader(entry.path.string());
                auto instance = loader.get_instance();
                return {std::move(instance), std::move(loader), idx, {}};
              } catch (...) {
                return {nullptr, std::nullopt, idx, std::current_exception()};
              }
            }));
      }

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
        locator_.add(result.instance);
        plugins_.push_back({std::move(result.instance), std::move(result.loader),
                            infos[result.info_idx].entry,
                            infos[result.info_idx].deps});
        rebuild_name_index();
        wire_instance_tracked(plugins_.back(), config_map);
    
      }
    }
  }

  // Register a pre-loaded plugin instance. Useful for testing without
  // the filesystem — construct your plugin, wire it through the manager.
  void add_plugin(std::shared_ptr<IPlugin> instance,
                  const PluginEntry& entry,
                  const ConfigMap& config_map = {}) {
    if (!instance) {
      throw std::runtime_error("add_plugin: instance must not be null");
    }
    if (name_index_.contains(entry.name)) {
      throw std::runtime_error("Plugin already loaded: " + entry.name);
    }

    // --- Conflict checks ---
    auto* conflict_aware = dynamic_cast<IConflictAware*>(instance.get());
    if (conflict_aware) {
      for (const auto& conflict_type : conflict_aware->conflicts()) {
        for (const auto& lp : plugins_) {
          if (lp.entry.type == conflict_type) {
            throw std::runtime_error(
                "Plugin '" + entry.name + "' conflicts with '" +
                lp.entry.name + "' (type: " + conflict_type + ")");
          }
        }
      }
    }
    // Check if any already-loaded plugin conflicts with this one.
    for (const auto& lp : plugins_) {
      auto* existing_conflict =
          dynamic_cast<IConflictAware*>(lp.instance.get());
      if (!existing_conflict) continue;
      for (const auto& conflict_type : existing_conflict->conflicts()) {
        if (conflict_type == entry.type) {
          throw std::runtime_error(
              "Plugin '" + lp.entry.name + "' conflicts with '" +
              entry.name + "' (type: " + entry.type + ")");
        }
      }
    }

    // --- Dependency + version checks ---
    // add_plugin is permissive about missing deps (provider may be added
    // later — the reverse version check on line ~407 catches mismatches
    // when the provider arrives). Version constraints are still validated
    // when the provider is already loaded.
    std::vector<std::string> deps;
    auto* dep_aware = dynamic_cast<IDependencyAware*>(instance.get());
    if (dep_aware) {
      for (const auto& raw_dep : dep_aware->dependencies()) {
        auto parsed = Dependency::parse(raw_dep);
        deps.push_back(parsed.type);

        // Check version constraint only if the provider is already loaded.
        for (const auto& lp : plugins_) {
          if (lp.entry.type == parsed.type) {
            if (parsed.op != Dependency::Op::any) {
              auto provider_version = SemVer::parse(lp.entry.version);
              if (!parsed.satisfied_by(provider_version)) {
                throw std::runtime_error(
                    "Plugin '" + entry.name + "' requires " + raw_dep +
                    " but '" + lp.entry.name + "' provides version " +
                    lp.entry.version);
              }
            }
            break;
          }
        }
      }
    }

    // --- Reverse version check ---
    // Existing plugins may depend on this plugin's type with a version constraint.
    for (const auto& lp : plugins_) {
      auto* existing_dep = dynamic_cast<IDependencyAware*>(lp.instance.get());
      if (!existing_dep) continue;
      for (const auto& raw_dep : existing_dep->dependencies()) {
        auto parsed = Dependency::parse(raw_dep);
        if (parsed.type == entry.type && parsed.op != Dependency::Op::any) {
          auto new_version = SemVer::parse(entry.version);
          if (!parsed.satisfied_by(new_version)) {
            throw std::runtime_error(
                "Plugin '" + lp.entry.name + "' requires " + raw_dep +
                " but '" + entry.name + "' provides version " +
                entry.version);
          }
        }
      }
    }

    // Pre-validate config before committing.
    // Uses resolve_plugin_config which validates and prepares defaults.
    // The actual configure() call happens later in wire_instance_tracked.
    (void)resolve_plugin_config(
        instance.get(), entry.name, config_map, /*skip_validation=*/false);

    locator_.add(instance);
    plugins_.push_back(
        {std::move(instance), std::nullopt, entry, std::move(deps)});
    rebuild_name_index();
    wire_instance_tracked(plugins_.back(), config_map, /*skip_validation=*/true);
    notify_loaded(entry.name, entry.type, entry);

  }

  // --- Per-plugin unload ---

  // Unload a single plugin by name. Reverse dependents are unloaded first
  // (cascade). Calls on_shutdown() on each unloaded plugin.
  // Throws std::runtime_error if the plugin is not loaded.
  void unload(const std::string& name) {
    auto it = name_index_.find(name);
    if (it == name_index_.end()) {
      throw std::runtime_error("Plugin not loaded: " + name);
    }

    // Find all reverse dependents (plugins that depend on this one).
    auto target_type = plugins_[it->second].entry.type;
    auto dep_indices = find_reverse_dependents(target_type);

    // Sort by descending index so deepest dependents are shut down first.
    std::sort(dep_indices.begin(), dep_indices.end(), std::greater<>());

    // Shutdown reverse dependents
    for (std::size_t idx : dep_indices) {
      shutdown_plugin(plugins_[idx]);
      notify_unloaded(plugins_[idx].entry.name, plugins_[idx].entry.type, plugins_[idx].entry);
    }

    // Shutdown the target
    shutdown_plugin(plugins_[it->second]);
    notify_unloaded(name, target_type, plugins_[it->second].entry);

    // Collect all indices to remove
    std::unordered_set<std::size_t> to_remove(dep_indices.begin(),
                                               dep_indices.end());
    to_remove.insert(it->second);

    // Rebuild plugins_ without the removed entries
    std::vector<LoadedPlugin> remaining;
    remaining.reserve(plugins_.size() - to_remove.size());
    for (std::size_t i = 0; i < plugins_.size(); ++i) {
      if (!to_remove.contains(i)) {
        remaining.push_back(std::move(plugins_[i]));
      }
    }
    plugins_ = std::move(remaining);
    rebuild_name_index();
    rebuild_locator();

  }

  // --- Per-plugin reload ---

  // Reload a single plugin by name. The plugin's library is re-loaded from
  // disk. If the plugin implements ISerializable, state is preserved across
  // the reload. Reverse dependents are shut down, then re-wired and
  // re-initialized to pick up the new service instance.
  // Throws std::runtime_error if the plugin is not loaded.
  void reload(const std::string& name, const ConfigMap& config_map = {}) {
    auto it = name_index_.find(name);
    if (it == name_index_.end()) {
      throw std::runtime_error("Plugin not loaded: " + name);
    }
    std::size_t target_idx = it->second;
    auto& target = plugins_[target_idx];

    // Load-then-swap: create the new instance FIRST.
    // If this throws, nothing has been shut down — manager stays consistent.
    std::shared_ptr<IPlugin> new_instance;
    std::optional<PluginLoader<IPlugin>> new_loader;

    if (target.entry.is_dynamic) {
      new_instance = DynamicPluginAdapter::load(target.entry.path.string());
    } else {
      PluginLoader<IPlugin> pl(target.entry.path.string());
      new_instance = pl.get_instance();
      new_loader = std::move(pl);
    }

    // New instance created successfully — safe to proceed with shutdown.

    // Find reverse dependents
    auto target_type = target.entry.type;
    auto dep_indices = find_reverse_dependents(target_type);
    std::sort(dep_indices.begin(), dep_indices.end(), std::greater<>());

    // Shutdown dependents in reverse order
    for (std::size_t idx : dep_indices) {
      shutdown_plugin(plugins_[idx]);
    }

    // Shutdown target
    shutdown_plugin(target);

    // Save state from old instance and restore into new
    auto* old_ser = dynamic_cast<ISerializable*>(target.instance.get());
    if (old_ser) {
      auto saved_state = old_ser->save_state();
      auto* new_ser = dynamic_cast<ISerializable*>(new_instance.get());
      if (new_ser) {
        new_ser->restore_state(saved_state);
      }
    }

    // Swap in the new instance
    target.instance = std::move(new_instance);
    target.loader = std::move(new_loader);

    // Rebuild locator so dependents see the new instance
    rebuild_locator();

    // Re-wire the target with subscription tracking
    wire_instance_tracked(target, config_map);

    // Re-wire dependents in forward (load) order with subscription tracking
    std::sort(dep_indices.begin(), dep_indices.end());
    for (std::size_t idx : dep_indices) {
      wire_instance_tracked(plugins_[idx], config_map);
    }

    notify_reloaded(name, target_type, target.entry);

  }

  // --- Health checks ---

  // Check health of all loaded, enabled plugins that implement IHealthAware.
  // Returns reports for unhealthy plugins, or all if include_healthy is true.
  // Disabled plugins are skipped.
  [[nodiscard]] std::vector<PluginHealthReport> check_health(
      bool include_healthy = false) const {
    std::vector<PluginHealthReport> reports;
    for (const auto& lp : plugins_) {
      if (!lp.enabled) continue;
      auto* health = dynamic_cast<IHealthAware*>(lp.instance.get());
      if (!health) continue;

      // Fast-path: skip healthy plugins when not requested.
      if (!include_healthy && health->is_healthy()) continue;

      // Slow-path: get full diagnostic status.
      reports.push_back({lp.entry.name, health->health_status()});
    }
    return reports;
  }

  // --- Enable/disable ---

  // Disable a plugin: calls on_shutdown(), unsubscribes from EventBus,
  // but keeps it registered. ServiceLocator queries via get_service<T>()
  // will skip it. Throws if not loaded.
  void disable(const std::string& name) {
    auto it = name_index_.find(name);
    if (it == name_index_.end()) {
      throw std::runtime_error("Plugin not loaded: " + name);
    }
    auto& lp = plugins_[it->second];
    if (!lp.enabled) return;  // already disabled

    // Cascade: disable reverse dependents first (same logic as unload).
    auto target_type = lp.entry.type;
    auto dep_indices = find_reverse_dependents(target_type);
    std::sort(dep_indices.begin(), dep_indices.end(), std::greater<>());
    for (std::size_t idx : dep_indices) {
      if (plugins_[idx].enabled) {
        disable_single(plugins_[idx]);
      }
    }

    disable_single(lp);
    rebuild_locator();
  }

  // Enable a previously disabled plugin: re-wires (configure, service locator,
  // event bus, on_init). Throws if not loaded.
  void enable(const std::string& name, const ConfigMap& config_map = {}) {
    auto it = name_index_.find(name);
    if (it == name_index_.end()) {
      throw std::runtime_error("Plugin not loaded: " + name);
    }
    auto& lp = plugins_[it->second];
    if (lp.enabled) return;  // already enabled

    lp.enabled = true;
    rebuild_locator();
    wire_instance_tracked(lp, config_map);
    notify_enabled(lp.entry.name, lp.entry.type, lp.entry);
  }

  // Check if a plugin is enabled.
  [[nodiscard]] bool is_enabled(const std::string& name) const {
    auto it = name_index_.find(name);
    if (it == name_index_.end()) return false;
    return plugins_[it->second].enabled;
  }

  // Get the first enabled service matching the given type.
  // Unlike locator().get<T>(), this skips disabled plugins.
  template <typename T>
  [[nodiscard]] std::shared_ptr<T> get_service(
      const std::string& type) const {
    for (const auto& lp : plugins_) {
      if (!lp.enabled) continue;
      if (lp.entry.type == type) {
        return std::dynamic_pointer_cast<T>(lp.instance);
      }
    }
    return nullptr;
  }

  // --- Plugin groups ---

  // Get names of all loaded plugins that have the given capability.
  // Uses IPluginMetadata::capabilities() for grouping.
  [[nodiscard]] std::vector<std::string> plugins_with_capability(
      const std::string& capability) const {
    std::vector<std::string> result;
    for (const auto& lp : plugins_) {
      auto* meta = dynamic_cast<IPluginMetadata*>(lp.instance.get());
      if (!meta) continue;
      for (const auto& cap : meta->capabilities()) {
        if (cap == capability) {
          result.push_back(lp.entry.name);
          break;
        }
      }
    }
    return result;
  }

  // Disable all plugins in a capability group.
  void disable_group(const std::string& capability) {
    for (const auto& name : plugins_with_capability(capability)) {
      if (is_enabled(name)) {
        disable(name);
      }
    }
  }

  // Enable all plugins in a capability group.
  void enable_group(const std::string& capability,
                    const ConfigMap& config_map = {}) {
    for (const auto& name : plugins_with_capability(capability)) {
      if (!is_enabled(name)) {
        enable(name, config_map);
      }
    }
  }

  // Unload all plugins in a capability group.
  void unload_group(const std::string& capability) {
    // Collect names first — unload modifies plugins_ vector.
    auto names = plugins_with_capability(capability);
    for (const auto& name : names) {
      if (is_loaded(name)) {
        unload(name);
      }
    }
  }

  // --- Config validation ---

  // Validate config (pure query, no mutation). Delegates to free function.
  [[nodiscard]] static std::vector<std::string> validate_config(
      IPlugin* instance, const PluginConfig& config) {
    return validate_plugin_config(instance, config);
  }

  // --- Observability hooks ---

  // Add an observer for plugin lifecycle events.
  void add_observer(PluginObserver* observer) {
    if (observer) observers_.push_back(observer);
  }

  // Remove an observer.
  void remove_observer(PluginObserver* observer) {
    if (!observer) return;
    std::erase(observers_, observer);
  }

  // --- Dependency graph ---

  // Returns a human-readable dependency graph of all loaded plugins.
  // Format: "PluginName (type) -> dep1, dep2"
  [[nodiscard]] std::vector<std::string> dependency_graph() const {
    std::vector<std::string> lines;
    for (const auto& lp : plugins_) {
      std::string line = lp.entry.name + " (" + lp.entry.type + ")";
      if (lp.deps.empty()) {
        line += " [no deps]";
      } else {
        line += " -> ";
        for (std::size_t i = 0; i < lp.deps.size(); ++i) {
          if (i > 0) line += ", ";
          line += lp.deps[i];
        }
      }
      lines.push_back(std::move(line));
    }
    return lines;
  }

  // --- Custom mixin wiring (extensibility) ---

  // Register a custom wiring step. Called for every plugin during wire_instance,
  // after built-in mixin injection (configure, service locator, event bus) and
  // before on_init(). Allows hosts to wire custom mixins without modifying
  // PluginManager source.
  using MixinWirer = std::function<void(const std::shared_ptr<IPlugin>&)>;

  void add_mixin_wirer(MixinWirer wirer) {
    if (wirer) custom_wirers_.push_back(std::move(wirer));
  }

  // Shut down all plugins in reverse load order.
  void shutdown() {
    for (auto it = plugins_.rbegin(); it != plugins_.rend(); ++it) {
      shutdown_plugin(*it);
    }
    plugins_.clear();
    name_index_.clear();
    locator_.clear();
    event_bus_.clear();
    load_errors_.clear();

  }

  [[nodiscard]] ServiceLocator& locator() { return locator_; }
  [[nodiscard]] const ServiceLocator& locator() const { return locator_; }

  [[nodiscard]] EventBus& event_bus() { return event_bus_; }
  [[nodiscard]] const EventBus& event_bus() const { return event_bus_; }

  [[nodiscard]] std::vector<std::shared_ptr<IPlugin>> instances() const {
    std::vector<std::shared_ptr<IPlugin>> result;
    result.reserve(plugins_.size());
    for (const auto& lp : plugins_) {
      result.push_back(lp.instance);
    }
    return result;
  }

  // Get a loaded plugin by name. Returns nullptr if not found.
  [[nodiscard]] std::shared_ptr<IPlugin> get_plugin(
      const std::string& name) const {
    auto it = name_index_.find(name);
    if (it == name_index_.end()) return nullptr;
    return plugins_[it->second].instance;
  }

  // Check if a plugin is currently loaded.
  [[nodiscard]] bool is_loaded(const std::string& name) const {
    return name_index_.contains(name);
  }

  // Names of plugins that would be unloaded if the given plugin is unloaded
  // (reverse dependents). Does not include the plugin itself.
  [[nodiscard]] std::vector<std::string> dependents_of(
      const std::string& name) const {
    auto it = name_index_.find(name);
    if (it == name_index_.end()) return {};

    auto indices = find_reverse_dependents(plugins_[it->second].entry.type);
    std::vector<std::string> result;
    result.reserve(indices.size());
    for (std::size_t idx : indices) {
      result.push_back(plugins_[idx].entry.name);
    }
    return result;
  }

  // Errors from best_effort loading (empty in strict mode).
  [[nodiscard]] const std::vector<ErrorRecord>& load_errors() const {
    return load_errors_;
  }

  // Access the internal loaded plugin records (for advanced use).
  [[nodiscard]] const std::vector<LoadedPlugin>& loaded_plugins() const {
    return plugins_;
  }

 private:
  ServiceLocator locator_;
  EventBus event_bus_;
  std::vector<LoadedPlugin> plugins_;
  std::unordered_map<std::string, std::size_t> name_index_;
  std::vector<ErrorRecord> load_errors_;
  std::vector<PluginObserver*> observers_;
  std::vector<MixinWirer> custom_wirers_;

  // --- Observer notification ---

  void notify_loaded(const std::string& name, const std::string& type, const PluginEntry& entry) {
    for (auto* obs : observers_) { try { obs->on_plugin_loaded(name, type, entry); } catch (...) {} }
  }
  void notify_unloaded(const std::string& name, const std::string& type, const PluginEntry& entry) {
    for (auto* obs : observers_) { try { obs->on_plugin_unloaded(name, type, entry); } catch (...) {} }
  }
  void notify_reloaded(const std::string& name, const std::string& type, const PluginEntry& entry) {
    for (auto* obs : observers_) { try { obs->on_plugin_reloaded(name, type, entry); } catch (...) {} }
  }
  void notify_enabled(const std::string& name, const std::string& type, const PluginEntry& entry) {
    for (auto* obs : observers_) { try { obs->on_plugin_enabled(name, type, entry); } catch (...) {} }
  }
  void notify_disabled(const std::string& name, const std::string& type, const PluginEntry& entry) {
    for (auto* obs : observers_) { try { obs->on_plugin_disabled(name, type, entry); } catch (...) {} }
  }

  // --- Name index ---

  void rebuild_name_index() {
    name_index_.clear();
    for (std::size_t i = 0; i < plugins_.size(); ++i) {
      name_index_[plugins_[i].entry.name] = i;
    }
  }

  // --- Locator rebuild ---

  // Only adds enabled plugins — disabled plugins are invisible to the locator.
  void rebuild_locator() {
    locator_.clear();
    for (const auto& lp : plugins_) {
      if (lp.enabled) {
        locator_.add(lp.instance);
      }
    }
  }

  // --- Disable helper ---

  // Disable a single plugin without cascading or rebuilding locator.
  void disable_single(LoadedPlugin& lp) {
    shutdown_plugin(lp);
    for (auto id : lp.subscription_ids) {
      event_bus_.unsubscribe(id);
    }
    lp.subscription_ids.clear();
    lp.enabled = false;
    notify_disabled(lp.entry.name, lp.entry.type, lp.entry);
  }

  // --- Reverse dependency graph ---

  // Find indices of all plugins that transitively depend on the given type.
  std::vector<std::size_t> find_reverse_dependents(
      const std::string& target_type) const {
    // Build reverse adjacency: type → indices that depend on it
    std::unordered_map<std::string, std::vector<std::size_t>> rev_adj;
    for (std::size_t i = 0; i < plugins_.size(); ++i) {
      for (const auto& dep : plugins_[i].deps) {
        rev_adj[dep].push_back(i);
      }
    }

    // BFS from target_type
    std::vector<std::size_t> result;
    std::unordered_set<std::size_t> visited;
    std::queue<std::string> bfs_queue;
    bfs_queue.push(target_type);

    while (!bfs_queue.empty()) {
      auto type = bfs_queue.front();
      bfs_queue.pop();

      auto it = rev_adj.find(type);
      if (it == rev_adj.end()) continue;

      for (std::size_t idx : it->second) {
        if (visited.insert(idx).second) {
          result.push_back(idx);
          bfs_queue.push(plugins_[idx].entry.type);
        }
      }
    }

    return result;
  }

  // --- Discovery ---

  struct DiscoveryResult {
    std::vector<DiscoveredPlugin> infos;
    std::vector<std::vector<std::size_t>> levels;
  };

  static DiscoveryResult discover_and_sort(
      PluginRegistry& registry,
      LoadPolicy policy = LoadPolicy::strict,
      std::vector<ErrorRecord>* errors_out = nullptr) {
    const auto& entries = registry.entries();
    if (entries.empty()) return {};

    std::vector<DiscoveredPlugin> infos;
    std::unordered_map<std::string, std::size_t> type_to_index;

    for (const auto& e : entries) {
      DiscoveredPlugin info;
      info.entry = e;

      // Probe for dependencies and conflicts (only typed plugins)
      if (!e.is_dynamic) {
        PluginLoader<IPlugin> probe_loader(e.path.string());
        auto probe_instance = probe_loader.get_instance();

        auto* dep_aware =
            dynamic_cast<IDependencyAware*>(probe_instance.get());
        if (dep_aware) {
          info.version_constraints = dep_aware->dependencies();
          for (const auto& raw_dep : info.version_constraints) {
            auto parsed = Dependency::parse(raw_dep);
            info.deps.push_back(parsed.type);
          }
        }

        auto* conflict_aware =
            dynamic_cast<IConflictAware*>(probe_instance.get());
        if (conflict_aware) {
          info.conflicts = conflict_aware->conflicts();
        }
      }

      auto [it, inserted] = type_to_index.try_emplace(e.type, infos.size());
      if (!inserted) {
        if (policy == LoadPolicy::strict) {
          throw std::runtime_error(
              "Duplicate plugin type '" + e.type + "': '" +
              infos[it->second].entry.name + "' and '" + e.name + "'");
        }
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

    check_version_constraints(infos, type_to_index, policy, errors_out);
    check_conflict_constraints(infos, type_to_index, policy, errors_out);

    auto levels = topological_sort_levels(infos, type_to_index);
    return {std::move(infos), std::move(levels)};
  }

  // --- Dependency cascade ---

  static bool has_failed_dep(
      const DiscoveredPlugin& info,
      const std::unordered_set<std::string>& failed_types) {
    for (const auto& dep : info.deps) {
      if (failed_types.contains(dep)) return true;
    }
    return false;
  }

  void record_cascade_skip(const DiscoveredPlugin& info,
                           std::unordered_set<std::string>& failed_types) {
    failed_types.insert(info.entry.type);
    load_errors_.push_back(
        {info.entry.path, "skipped: dependency unavailable"});
  }

  // --- Loading + wiring ---

  void try_load_and_wire(const PluginEntry& entry,
                         const std::vector<std::string>& deps,
                         const ConfigMap& config_map, LoadPolicy policy,
                         std::unordered_set<std::string>& failed_types) {
    if (policy == LoadPolicy::strict) {
      load_and_wire(entry, deps, config_map);
    } else {
      try {
        load_and_wire(entry, deps, config_map);
      } catch (const std::exception& e) {
        failed_types.insert(entry.type);
        load_errors_.push_back({entry.path, e.what()});
      }
    }
  }

  // Resolve, validate, and apply config for a plugin.
  // Returns the final config if the plugin is configurable, or nullopt.
  static std::optional<PluginConfig> resolve_plugin_config(
      IPlugin* instance, const std::string& name,
      const ConfigMap& config_map, bool skip_validation) {
    auto* configurable = dynamic_cast<IConfigurable*>(instance);
    if (!configurable) return std::nullopt;

    auto cfg_it = config_map.find(name);
    PluginConfig config = (cfg_it != config_map.end())
                              ? cfg_it->second
                              : PluginConfig{};

    if (!skip_validation) {
      auto errors = validate_plugin_config(instance, config);
      if (!errors.empty()) {
        std::string msg = "Config validation failed for '" + name + "': ";
        for (std::size_t i = 0; i < errors.size(); ++i) {
          if (i > 0) msg += "; ";
          msg += errors[i];
        }
        throw std::runtime_error(msg);
      }
    }

    apply_config_defaults(instance, config);
    return config;
  }

  // Wire + track EventBus subscription IDs (for enable/disable).
  //
  // Relies on EventBus::next_id_ being monotonically increasing with no
  // reuse: all IDs allocated between [before, after) belong to this plugin.
  // This holds because PluginManager is single-threaded (or exclusively
  // locked via ThreadSafe), and wire_instance may call on_init() /
  // set_event_bus() which subscribe — those subscriptions correctly belong
  // to this plugin. Unsubscribes during wiring don't break the invariant
  // because IDs are never recycled.
  void wire_instance_tracked(LoadedPlugin& lp, const ConfigMap& config_map,
                              bool skip_validation = false) {
    auto before = event_bus_.next_subscription_id();
    wire_instance(lp.instance, lp.entry, config_map, skip_validation);
    auto after = event_bus_.next_subscription_id();

    lp.subscription_ids.clear();
    for (auto id = before; id < after; ++id) {
      lp.subscription_ids.push_back(id);
    }
  }

 protected:
  // --- Extensibility points (override in subclasses) ---

  // Wire all opt-in mixins on an instance.
  // Order: validate+configure → inject services → inject events → custom wirers → init.
  virtual void wire_instance(const std::shared_ptr<IPlugin>& instance,
                     const PluginEntry& entry, const ConfigMap& config_map,
                     bool skip_validation = false) {
    auto resolved = resolve_plugin_config(
        instance.get(), entry.name, config_map, skip_validation);
    if (resolved) {
      dynamic_cast<IConfigurable*>(instance.get())->configure(*resolved);
    }

    auto* aware = dynamic_cast<IServiceAware*>(instance.get());
    if (aware) {
      aware->set_service_locator(locator_);
    }

    auto* event_aware = dynamic_cast<IEventAware*>(instance.get());
    if (event_aware) {
      event_aware->set_event_bus(event_bus_);
    }

    // Custom mixin wirers registered by the host.
    for (const auto& wirer : custom_wirers_) {
      wirer(instance);
    }

    // on_init() must be last — plugin may use injected services/events.
    auto* lifecycle = dynamic_cast<ILifecycleAware*>(instance.get());
    if (lifecycle) {
      lifecycle->on_init();
    }
  }

  // Calls on_shutdown() if the plugin implements ILifecycleAware.
  // Swallows exceptions — one bad plugin must not prevent others from
  // shutting down (same principle as EventBus::publish).
  virtual void shutdown_plugin(LoadedPlugin& lp) {
    auto* lifecycle = dynamic_cast<ILifecycleAware*>(lp.instance.get());
    if (lifecycle) {
      try {
        lifecycle->on_shutdown();
      } catch (...) {
      }
    }
  }

  // Load a plugin from disk and wire it into the manager.
  virtual void load_and_wire(const PluginEntry& entry,
                     const std::vector<std::string>& deps,
                     const ConfigMap& config_map) {
    std::shared_ptr<IPlugin> instance;
    std::optional<PluginLoader<IPlugin>> loader;

    if (entry.is_dynamic) {
      instance = DynamicPluginAdapter::load(entry.path.string());
    } else {
      PluginLoader<IPlugin> pl(entry.path.string());
      instance = pl.get_instance();
      loader = std::move(pl);
    }

    locator_.add(instance);
    plugins_.push_back(
        {std::move(instance), std::move(loader), entry, deps});
    rebuild_name_index();
    wire_instance_tracked(plugins_.back(), config_map);
    notify_loaded(entry.name, entry.type, entry);
  }
};

// --- ObserverGuard inline implementation ---
// Defined here because it needs PluginManager to be complete.

inline ObserverGuard::ObserverGuard(PluginManager& manager,
                                    PluginObserver* observer)
    : manager_(&manager), observer_(observer) {
  if (observer_) manager_->add_observer(observer_);
}

inline ObserverGuard::~ObserverGuard() { release(); }

inline void ObserverGuard::release() {
  if (observer_) {
    manager_->remove_observer(observer_);
    observer_ = nullptr;
  }
}

}  // namespace plugin_arch
