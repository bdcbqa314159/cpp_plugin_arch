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
#include "IConfigurable.hpp"
#include "IDependencyAware.hpp"
#include "IEventAware.hpp"
#include "ILifecycleAware.hpp"
#include "IPlugin.hpp"
#include "ISerializable.hpp"
#include "IServiceAware.hpp"
#include "PluginLoader.hpp"
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
struct PluginInfo {
  PluginEntry entry;
  std::vector<std::string> deps;      // type strings (version stripped, for topo sort)
  std::vector<std::string> raw_deps;  // original strings (for version checking)
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

  // State for a loaded plugin — tracks everything needed for unload/reload.
  struct LoadedPlugin {
    std::shared_ptr<IPlugin> instance;
    std::optional<PluginLoader<IPlugin>> loader;  // nullopt for add_plugin()
                                                  // and C ABI adapters
    PluginEntry entry;
    std::vector<std::string> deps;  // dependency type strings
  };

  // Scan a directory and load all discovered plugins in dependency order.
  // Supports both typed (IPlugin) and C ABI (PluginDescriptor) plugins.
  // config_map provides optional per-plugin configuration keyed by plugin name.
  // policy: strict (throw on first failure) or best_effort (skip, record errors).
  void load_all(const std::filesystem::path& directory,
                const ConfigMap& config_map = {},
                LoadPolicy policy = LoadPolicy::strict) {
    load_errors_.clear();

    auto [infos, levels] = discover_and_sort(directory, policy, &load_errors_);

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
    load_errors_.clear();

    auto [infos, levels] = discover_and_sort(directory, policy, &load_errors_);

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
        wire_instance(result.instance, infos[result.info_idx].entry,
                      config_map);
        locator_.add(result.instance);
        plugins_.push_back({std::move(result.instance), std::move(result.loader),
                            infos[result.info_idx].entry,
                            infos[result.info_idx].deps});
        rebuild_name_index();
        instances_dirty_ = true;
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

    std::vector<std::string> deps;
    auto* dep_aware = dynamic_cast<IDependencyAware*>(instance.get());
    if (dep_aware) {
      for (const auto& raw_dep : dep_aware->dependencies()) {
        deps.push_back(Dependency::parse(raw_dep).type);
      }
    }

    plugins_.push_back(
        {std::move(instance), std::nullopt, entry, std::move(deps)});
    rebuild_name_index();
    instances_dirty_ = true;
  }

  // --- Per-plugin unload (A1) ---

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
    }

    // Shutdown the target
    shutdown_plugin(plugins_[it->second]);

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
    instances_dirty_ = true;
  }

  // --- Per-plugin reload (A1) ---

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

    // Re-wire the target
    wire_instance(target.instance, target.entry, config_map);

    // Re-wire dependents in forward (load) order
    std::sort(dep_indices.begin(), dep_indices.end());
    for (std::size_t idx : dep_indices) {
      wire_instance(plugins_[idx].instance, plugins_[idx].entry, config_map);
    }

    instances_dirty_ = true;
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
    instances_dirty_ = true;
  }

  [[nodiscard]] ServiceLocator& locator() { return locator_; }
  [[nodiscard]] const ServiceLocator& locator() const { return locator_; }

  [[nodiscard]] EventBus& event_bus() { return event_bus_; }
  [[nodiscard]] const EventBus& event_bus() const { return event_bus_; }

  [[nodiscard]] const std::vector<std::shared_ptr<IPlugin>>& instances()
      const {
    if (instances_dirty_) {
      instances_cache_.clear();
      instances_cache_.reserve(plugins_.size());
      for (const auto& lp : plugins_) {
        instances_cache_.push_back(lp.instance);
      }
      instances_dirty_ = false;
    }
    return instances_cache_;
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
  mutable std::vector<std::shared_ptr<IPlugin>> instances_cache_;
  mutable bool instances_dirty_ = true;
  std::vector<ErrorRecord> load_errors_;

  // --- Name index ---

  void rebuild_name_index() {
    name_index_.clear();
    for (std::size_t i = 0; i < plugins_.size(); ++i) {
      name_index_[plugins_[i].entry.name] = i;
    }
  }

  // --- Locator rebuild ---

  void rebuild_locator() {
    locator_.clear();
    for (const auto& lp : plugins_) {
      locator_.add(lp.instance);
    }
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

  // --- Plugin shutdown ---

  static void shutdown_plugin(LoadedPlugin& lp) {
    auto* lifecycle = dynamic_cast<ILifecycleAware*>(lp.instance.get());
    if (lifecycle) {
      lifecycle->on_shutdown();
    }
  }

  // --- Discovery ---

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

      // Probe for dependencies (only typed plugins can declare deps)
      if (!e.is_dynamic) {
        PluginLoader<IPlugin> probe_loader(e.path.string());
        auto probe_instance = probe_loader.get_instance();
        auto* dep_aware =
            dynamic_cast<IDependencyAware*>(probe_instance.get());
        if (dep_aware) {
          info.raw_deps = dep_aware->dependencies();
          for (const auto& raw_dep : info.raw_deps) {
            auto parsed = Dependency::parse(raw_dep);
            info.deps.push_back(parsed.type);
          }
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

    // --- Version constraint checking (A2) ---
    // Uses raw_deps saved during probe — no re-loading needed.
    for (const auto& info : infos) {
      for (const auto& raw_dep : info.raw_deps) {
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
      }
    }

    auto levels = topological_sort_levels(infos, type_to_index);
    return {std::move(infos), std::move(levels)};
  }

  // --- Dependency cascade ---

  static bool has_failed_dep(
      const PluginInfo& info,
      const std::unordered_set<std::string>& failed_types) {
    for (const auto& dep : info.deps) {
      if (failed_types.contains(dep)) return true;
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

  void load_and_wire(const PluginEntry& entry,
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

    wire_instance(instance, entry, config_map);
    locator_.add(instance);
    plugins_.push_back(
        {std::move(instance), std::move(loader), entry, deps});
    rebuild_name_index();
    instances_dirty_ = true;
  }

  // Wire all opt-in mixins on an instance.
  // Order matters: configure → inject services → inject events → init.
  void wire_instance(std::shared_ptr<IPlugin>& instance,
                     const PluginEntry& entry, const ConfigMap& config_map) {
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
