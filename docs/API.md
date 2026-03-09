# plugin_arch API Reference

**Namespace:** `plugin_arch` (unless otherwise noted)
**Umbrella header:** `#include "plugin_arch/plugin_arch.hpp"`

---

## Core

### IPlugin
**Header:** `plugin_arch/IPlugin.hpp`

Abstract base interface for all plugins. Every plugin must inherit this so the host can query identity before casting to a domain-specific interface.

| Method | Signature | Description |
|--------|-----------|-------------|
| `name` | `virtual const std::string& name() const = 0` | Plugin's human-readable name. |
| `version` | `virtual const std::string& version() const = 0` | Plugin version string (e.g. `"1.2.3"`). |
| `type` | `virtual const std::string& type() const = 0` | Plugin type/category string for service lookup. |
| *(destructor)* | `virtual ~IPlugin() = default` | Virtual destructor. |

---

### PluginLoader\<T\>
**Header:** `plugin_arch/PluginLoader.hpp`

Typed RAII plugin loader. Opens a shared library at construction, resolves `allocator`/`deallocator` symbols, and returns `shared_ptr<T>` instances with custom deleters that call back into the plugin library.

**Template parameter:** `T` -- the interface type to cast to (must be reachable via `dynamic_cast` from `IPlugin*`).

**Type aliases:**

| Alias | Definition |
|-------|------------|
| `AllocFunc` | `IPlugin* (*)()` |
| `DeallocFunc` | `void (*)(IPlugin*)` |

| Method | Signature | Description |
|--------|-----------|-------------|
| *(constructor)* | `explicit PluginLoader(std::string library_path, std::string alloc_symbol = "allocator", std::string dealloc_symbol = "deallocator")` | Opens library, resolves symbols. Throws `LoadError`/`SymbolError` on failure. |
| `get_instance` | `[[nodiscard]] std::shared_ptr<T> get_instance()` | Allocates a new plugin instance via the factory, `dynamic_cast`s to `T*`. Throws `AllocationError` or `InterfaceError`. |
| `path` | `[[nodiscard]] const std::string& path() const` | Returns the library file path. |

Move-only (copy deleted, move defaulted).

---

### PluginExport (REGISTER_PLUGIN macro)
**Header:** `plugin_arch/PluginExport.hpp`

Macro that generates the `extern "C"` allocator/deallocator boilerplate for a plugin class.

**Macro:**
```cpp
REGISTER_PLUGIN(ClassName)
```
Expands to:
- `extern "C" EXPORTED IPlugin* allocator()` -- `new ClassName()`
- `extern "C" EXPORTED void deallocator(IPlugin* ptr)` -- `delete ptr`

---

### DynamicLibrary
**Header:** `plugin_arch/DynamicLibrary.hpp`

Non-templated RAII wrapper around `dlopen`/`dlclose` (POSIX) or `LoadLibrary`/`FreeLibrary` (Windows). Makes no assumptions about exported symbols.

| Method | Signature | Description |
|--------|-----------|-------------|
| *(constructor)* | `explicit DynamicLibrary(std::string library_path)` | Opens the library. Throws `LoadError` on failure. |
| *(destructor)* | `~DynamicLibrary()` | Closes the library handle. |
| `resolve<Func>` | `template <typename Func> [[nodiscard]] Func resolve(const std::string& symbol) const` | Resolves a symbol by name and casts to `Func` (must be a function pointer type). Throws `SymbolError` if not found. |
| `has` | `[[nodiscard]] bool has(const std::string& symbol) const` | Checks whether a symbol exists without throwing. |
| `path` | `[[nodiscard]] const std::string& path() const` | Returns the library file path. |

Move-only (copy deleted, move implemented).

---

### PluginDescriptor / FunctionEntry
**Header:** `plugin_arch/PluginDescriptor.hpp`

POD structs for C ABI plugins that export a `plugin_describe()` function instead of `allocator`/`deallocator`.

**`struct FunctionEntry`:**

| Field | Type | Description |
|-------|------|-------------|
| `name` | `const char*` | Function name (e.g. `"math_add"`). |
| `signature` | `const char*` | Informational signature string (e.g. `"double(double, double)"`). |

**`struct PluginDescriptor`:**

| Field | Type | Description |
|-------|------|-------------|
| `name` | `const char*` | Plugin name. |
| `version` | `const char*` | Plugin version. |
| `function_count` | `int` | Number of entries in the `functions` array. |
| `functions` | `const FunctionEntry*` | Array of exported functions. |

**Macro:**
```cpp
REGISTER_DYNAMIC_PLUGIN(plugin_name, plugin_version, ...)
```
Expands to `extern "C" EXPORTED const PluginDescriptor* plugin_describe()` that returns a static descriptor built from the variadic `{name, signature}` pairs.

---

### DynamicPluginAdapter
**Header:** `plugin_arch/DynamicPluginAdapter.hpp`

Adapter that wraps a C ABI plugin (`PluginDescriptor`) as an `IPlugin`, also implementing `ILifecycleAware` and `IConfigurable`. Probes for optional `plugin_init`, `plugin_shutdown`, and `plugin_configure` C callbacks via `dlsym`.

**Inherits:** `IPlugin`, `ILifecycleAware`, `IConfigurable`

**Type aliases:**

| Alias | Definition |
|-------|------------|
| `InitFunc` | `void (*)()` |
| `ShutdownFunc` | `void (*)()` |
| `ConfigureFunc` | `void (*)(int count, const char** keys, const char** values)` |

| Method | Signature | Description |
|--------|-----------|-------------|
| `load` | `[[nodiscard]] static std::shared_ptr<DynamicPluginAdapter> load(const std::string& path)` | Factory: constructs a shared adapter from a library path. |
| *(constructor)* | `explicit DynamicPluginAdapter(const std::string& path)` | Opens library, resolves `plugin_describe`, probes optional callbacks. |
| `name` | `const std::string& name() const override` | From `IPlugin`. |
| `version` | `const std::string& version() const override` | From `IPlugin`. |
| `type` | `const std::string& type() const override` | From `IPlugin` (defaults to `name`). |
| `on_init` | `void on_init() override` | Calls C `plugin_init()` if present. |
| `on_shutdown` | `void on_shutdown() override` | Calls C `plugin_shutdown()` if present. |
| `configure` | `void configure(const PluginConfig& config) override` | Marshals config to C arrays, calls `plugin_configure()` if present. |
| `get_function<Func>` | `template <typename Func> [[nodiscard]] Func get_function(const std::string& func_name) const` | Resolves a C function by name from the library. |
| `has_function` | `[[nodiscard]] bool has_function(const std::string& func_name) const` | Checks if a C function exists. |
| `descriptor` | `[[nodiscard]] const PluginDescriptor& descriptor() const` | Returns the raw `PluginDescriptor`. |
| `library` | `[[nodiscard]] const std::shared_ptr<DynamicLibrary>& library() const` | Returns the underlying `DynamicLibrary`. |

---

## Discovery & Orchestration

### PluginRegistry
**Header:** `plugin_arch/PluginRegistry.hpp`

Scans directories for shared libraries, probes each for plugin metadata, and indexes the results. Discovers both typed plugins (`allocator`/`deallocator`) and C ABI plugins (`plugin_describe`).

**`struct PluginEntry`:**

| Field | Type | Description |
|-------|------|-------------|
| `path` | `std::filesystem::path` | Library file path. |
| `name` | `std::string` | Plugin name. |
| `version` | `std::string` | Plugin version. |
| `type` | `std::string` | Plugin type string. |
| `author` | `std::string` | From `IPluginMetadata` (if implemented). |
| `description` | `std::string` | From `IPluginMetadata` (if implemented). |
| `license` | `std::string` | From `IPluginMetadata` (if implemented). |
| `capabilities` | `std::vector<std::string>` | From `IPluginMetadata` (if implemented). |
| `is_dynamic` | `bool` | `true` for C ABI plugins, `false` for typed. |

**`struct ErrorRecord`:**

| Field | Type | Description |
|-------|------|-------------|
| `path` | `std::filesystem::path` | Library that failed. |
| `reason` | `std::string` | Error message. |

| Method | Signature | Description |
|--------|-----------|-------------|
| `scan` | `[[nodiscard]] std::size_t scan(const std::filesystem::path& directory)` | Scans directory, appends discovered entries. Returns count of new plugins found. Throws `LoadError` if path is not a directory. |
| `get_all` | `[[nodiscard]] std::vector<PluginEntry> get_all(const std::string& type) const` | Returns all entries matching a type string. |
| `get` | `[[nodiscard]] std::optional<PluginEntry> get(const std::string& name) const` | Returns first entry matching a name, or `nullopt`. |
| `entries` | `[[nodiscard]] const std::vector<PluginEntry>& entries() const` | All discovered entries. |
| `errors` | `[[nodiscard]] const std::vector<ErrorRecord>& errors() const` | Libraries that failed to probe. |
| `clear` | `void clear()` | Clears all entries and errors. |

---

### PluginManager
**Header:** `plugin_arch/PluginManager.hpp`

High-level orchestrator. Composes registry, loader, service locator, event bus, and all opt-in mixins. Handles discovery, dependency ordering (topological sort), version constraint checking, loading, wiring, unload/reload with cascade, enable/disable, health checks, config validation, conflict detection, capability-based groups, and observer notifications.

**`enum class LoadPolicy`:**

| Value | Description |
|-------|-------------|
| `strict` | Throw on first failure (default). |
| `best_effort` | Skip failed plugins, record errors. |

**`struct PluginInfo`:**

| Field | Type | Description |
|-------|------|-------------|
| `entry` | `PluginEntry` | Plugin metadata. |
| `deps` | `std::vector<std::string>` | Dependency type strings (version stripped). |
| `raw_deps` | `std::vector<std::string>` | Original dependency strings (with version constraints). |
| `conflicts` | `std::vector<std::string>` | Conflicting type strings. |

**Type aliases:**

| Alias | Definition |
|-------|------------|
| `ConfigMap` | `std::unordered_map<std::string, PluginConfig>` |

**`struct PluginHealthReport`:**

| Field | Type |
|-------|------|
| `plugin_name` | `std::string` |
| `status` | `HealthStatus` |

**`struct LoadedPlugin`:**

| Field | Type | Description |
|-------|------|-------------|
| `instance` | `std::shared_ptr<IPlugin>` | The plugin instance. |
| `loader` | `std::optional<PluginLoader<IPlugin>>` | `nullopt` for `add_plugin()` and C ABI adapters. |
| `entry` | `PluginEntry` | Metadata. |
| `deps` | `std::vector<std::string>` | Dependency type strings. |
| `enabled` | `bool` | Whether the plugin is active. |
| `subscription_ids` | `std::vector<EventBus::SubscriptionId>` | Tracked subscriptions for disable. |

| Method | Signature | Description |
|--------|-----------|-------------|
| `load_all` | `void load_all(const std::filesystem::path& directory, const ConfigMap& config_map = {}, LoadPolicy policy = LoadPolicy::strict)` | Scan directory, topologically sort, load and wire all plugins. |
| `load_all_parallel` | `void load_all_parallel(const std::filesystem::path& directory, const ConfigMap& config_map = {}, LoadPolicy policy = LoadPolicy::strict)` | Same as `load_all` but parallelizes loading within each topological level via `std::async`. |
| `add_plugin` | `void add_plugin(std::shared_ptr<IPlugin> instance, const PluginEntry& entry, const ConfigMap& config_map = {})` | Register a pre-loaded plugin. Performs conflict, dependency, version, and config schema checks. |
| `unload` | `void unload(const std::string& name)` | Unload a plugin by name; reverse dependents are cascade-unloaded first. |
| `reload` | `void reload(const std::string& name, const ConfigMap& config_map = {})` | Reload from disk with load-then-swap. Preserves state via `ISerializable`. Re-wires dependents. |
| `disable` | `void disable(const std::string& name)` | Calls `on_shutdown()`, unsubscribes from EventBus, cascades to dependents. Plugin stays registered but invisible to `get_service`. |
| `enable` | `void enable(const std::string& name, const ConfigMap& config_map = {})` | Re-wires and re-initializes a disabled plugin. |
| `shutdown` | `void shutdown()` | Shuts down all plugins in reverse load order, clears all state. |
| `is_loaded` | `[[nodiscard]] bool is_loaded(const std::string& name) const` | Check if a plugin is currently loaded. |
| `is_enabled` | `[[nodiscard]] bool is_enabled(const std::string& name) const` | Check if a plugin is enabled. |
| `get_plugin` | `[[nodiscard]] std::shared_ptr<IPlugin> get_plugin(const std::string& name) const` | Get a loaded plugin by name. Returns `nullptr` if not found. |
| `get_service<T>` | `template <typename T> [[nodiscard]] std::shared_ptr<T> get_service(const std::string& type) const` | Get first enabled service matching type (skips disabled plugins). |
| `check_health` | `[[nodiscard]] std::vector<PluginHealthReport> check_health(bool include_healthy = false) const` | Poll `IHealthAware` plugins. |
| `dependents_of` | `[[nodiscard]] std::vector<std::string> dependents_of(const std::string& name) const` | Names of plugins that transitively depend on the given plugin. |
| `plugins_with_capability` | `[[nodiscard]] std::vector<std::string> plugins_with_capability(const std::string& capability) const` | Names of plugins with a given capability (via `IPluginMetadata`). |
| `disable_group` | `void disable_group(const std::string& capability)` | Disable all plugins in a capability group. |
| `enable_group` | `void enable_group(const std::string& capability, const ConfigMap& config_map = {})` | Enable all plugins in a capability group. |
| `unload_group` | `void unload_group(const std::string& capability)` | Unload all plugins in a capability group. |
| `validate_config` | `[[nodiscard]] static std::vector<std::string> validate_config(IPlugin* instance, PluginConfig& config)` | Validate config against `IConfigSchema`. Applies defaults. Returns error messages. |
| `dependency_graph` | `[[nodiscard]] std::vector<std::string> dependency_graph() const` | Human-readable dependency graph lines: `"Name (type) -> dep1, dep2"`. |
| `add_observer` | `void add_observer(PluginObserver* observer)` | Register a lifecycle observer. |
| `remove_observer` | `void remove_observer(PluginObserver* observer)` | Unregister a lifecycle observer. |
| `locator` | `ServiceLocator& locator()` | Access the internal `ServiceLocator`. |
| `event_bus` | `EventBus& event_bus()` | Access the internal `EventBus`. |
| `instances` | `[[nodiscard]] const std::vector<std::shared_ptr<IPlugin>>& instances() const` | All plugin instances. |
| `load_errors` | `[[nodiscard]] const std::vector<ErrorRecord>& load_errors() const` | Errors from `best_effort` loading. |
| `loaded_plugins` | `[[nodiscard]] const std::vector<LoadedPlugin>& loaded_plugins() const` | Internal loaded plugin records. |

**Free function:**

| Function | Signature | Description |
|----------|-----------|-------------|
| `topological_sort_levels` | `std::vector<std::vector<std::size_t>> topological_sort_levels(const std::vector<PluginInfo>& infos, const std::unordered_map<std::string, std::size_t>& type_to_index)` | Kahn's algorithm. Returns indices grouped by topological level. Throws on cycles or missing deps. |

---

### HotPluginLoader\<T\>
**Header:** `plugin_arch/HotPluginLoader.hpp`

Hot-reload wrapper around `PluginLoader`. Detects changes via `std::filesystem::last_write_time()` polling. Old instances keep their library alive via `shared_ptr` custom deleter. Preserves state across reloads via `ISerializable`.

**Template parameter:** `T` -- the plugin interface type.

| Method | Signature | Description |
|--------|-----------|-------------|
| *(constructor)* | `explicit HotPluginLoader(std::string library_path, std::string alloc_symbol = "allocator", std::string dealloc_symbol = "deallocator")` | Opens library, creates initial instance. |
| `get_instance` | `[[nodiscard]] std::shared_ptr<T> get_instance() const` | Returns the current plugin instance. |
| `check_and_reload` | `[[nodiscard]] bool check_and_reload()` | Polls file mtime; reloads if changed. Returns `true` on reload. |
| `reload` | `void reload()` | Force reload regardless of mtime. Preserves state via `ISerializable`. |
| `library_path` | `[[nodiscard]] const std::string& library_path() const` | Returns the library file path. |

Move-only (copy deleted, move defaulted).

---

## Opt-in Mixins

All mixins are detected via `dynamic_cast` by the host/manager. Plugins opt in through multiple inheritance.

### ILifecycleAware
**Header:** `plugin_arch/ILifecycleAware.hpp`

| Method | Signature | Description |
|--------|-----------|-------------|
| `on_init` | `virtual void on_init() = 0` | Called after construction and service injection. |
| `on_shutdown` | `virtual void on_shutdown() = 0` | Called before destruction. |

---

### IConfigurable
**Header:** `plugin_arch/IConfigurable.hpp`

**Type alias:** `using PluginConfig = std::unordered_map<std::string, std::string>`

| Method | Signature | Description |
|--------|-----------|-------------|
| `configure` | `virtual void configure(const PluginConfig& config) = 0` | Accept key-value configuration parameters. |

---

### IConfigSchema
**Header:** `plugin_arch/IConfigSchema.hpp`

**`struct ConfigKeyDef`:**

| Field | Type | Description |
|-------|------|-------------|
| `key` | `std::string` | Config key name. |
| `required` | `bool` | Whether the key is required (default `false`). |
| `default_value` | `std::string` | Default value when absent and not required. |
| `description` | `std::string` | Human-readable description. |

| Method | Signature | Description |
|--------|-----------|-------------|
| `config_schema` | `virtual std::vector<ConfigKeyDef> config_schema() const = 0` | Returns accepted config keys with metadata. |

---

### IDependencyAware
**Header:** `plugin_arch/IDependencyAware.hpp`

| Method | Signature | Description |
|--------|-----------|-------------|
| `dependencies` | `virtual std::vector<std::string> dependencies() const = 0` | Returns type strings (optionally with version constraints) of required services. |

---

### IServiceAware
**Header:** `plugin_arch/IServiceAware.hpp`

| Method | Signature | Description |
|--------|-----------|-------------|
| `set_service_locator` | `virtual void set_service_locator(ServiceLocator& locator) = 0` | Called by host to inject the service locator. |

---

### IEventAware
**Header:** `plugin_arch/IEventAware.hpp`

| Method | Signature | Description |
|--------|-----------|-------------|
| `set_event_bus` | `virtual void set_event_bus(EventBus& bus) = 0` | Called by host to inject the event bus. |

---

### IHealthAware
**Header:** `plugin_arch/IHealthAware.hpp`

**`struct HealthStatus`:**

| Field | Type | Description |
|-------|------|-------------|
| `healthy` | `bool` | `true` if healthy (default). |
| `message` | `std::string` | Diagnostic message (empty = fine). |

| Method | Signature | Description |
|--------|-----------|-------------|
| `is_healthy` | `virtual bool is_healthy() const = 0` | Fast-path boolean check. |
| `health_status` | `virtual HealthStatus health_status() const = 0` | Richer status with diagnostic message. |

---

### IConflictAware
**Header:** `plugin_arch/IConflictAware.hpp`

| Method | Signature | Description |
|--------|-----------|-------------|
| `conflicts` | `virtual std::vector<std::string> conflicts() const = 0` | Returns type strings of plugins this plugin conflicts with. |

---

### IPluginMetadata
**Header:** `plugin_arch/IPluginMetadata.hpp`

| Method | Signature | Description |
|--------|-----------|-------------|
| `author` | `virtual const std::string& author() const = 0` | Plugin author. |
| `description` | `virtual const std::string& description() const = 0` | Plugin description. |
| `license` | `virtual const std::string& license() const = 0` | Plugin license. |
| `capabilities` | `virtual std::vector<std::string> capabilities() const` | Capability tags for grouping (default: empty). |

---

### ISerializable
**Header:** `plugin_arch/ISerializable.hpp`

| Method | Signature | Description |
|--------|-----------|-------------|
| `save_state` | `virtual std::string save_state() const = 0` | Serialize current state to a string. |
| `restore_state` | `virtual void restore_state(const std::string& data) = 0` | Restore state from a previously saved string. |

---

### PluginObserver
**Header:** `plugin_arch/PluginObserver.hpp`

Observer interface for plugin lifecycle events. All callbacks have default no-op implementations.

| Method | Signature | Description |
|--------|-----------|-------------|
| `on_plugin_loaded` | `virtual void on_plugin_loaded(const std::string& name, const std::string& type)` | Called when a plugin is loaded. |
| `on_plugin_unloaded` | `virtual void on_plugin_unloaded(const std::string& name, const std::string& type)` | Called when a plugin is unloaded. |
| `on_plugin_reloaded` | `virtual void on_plugin_reloaded(const std::string& name, const std::string& type)` | Called when a plugin is reloaded. |
| `on_plugin_enabled` | `virtual void on_plugin_enabled(const std::string& name, const std::string& type)` | Called when a plugin is enabled. |
| `on_plugin_disabled` | `virtual void on_plugin_disabled(const std::string& name, const std::string& type)` | Called when a plugin is disabled. |

---

## Communication

### ServiceLocator
**Header:** `plugin_arch/ServiceLocator.hpp`

Lets plugins discover and call each other at runtime. Stores `weak_ptr<IPlugin>` -- does not own plugins.

| Method | Signature | Description |
|--------|-----------|-------------|
| `add` | `void add(const std::shared_ptr<IPlugin>& service)` | Register a plugin as a service (weak reference). |
| `get<T>` | `template <typename T> [[nodiscard]] std::shared_ptr<T> get(const std::string& type) const` | Get first service matching type, `dynamic_pointer_cast` to `T`. Returns `nullptr` if not found. |
| `get_all<T>` | `template <typename T> [[nodiscard]] std::vector<std::shared_ptr<T>> get_all(const std::string& type) const` | Get all services matching type. |
| `cleanup` | `void cleanup()` | Remove expired entries (destroyed plugins). |
| `clear` | `void clear()` | Remove all entries. |
| `size` | `[[nodiscard]] std::size_t size() const` | Number of registered entries (including expired). |

---

### EventBus
**Header:** `plugin_arch/EventBus.hpp`

Publish-subscribe event bus with three APIs: string-based, type-safe, and vetoable. All support priority dispatch (higher = first). Synchronous dispatch.

**Type aliases:**

| Alias | Definition |
|-------|------------|
| `Handler` | `std::function<void(const std::string& topic, const std::string& payload)>` |
| `SubscriptionId` | `std::size_t` |
| `VetoHandler` | `std::function<bool(const std::string& topic, const std::string& payload)>` |
| `TypedHandler<T>` | `std::function<void(const std::string& topic, const T& event)>` |

**Constants:**

| Constant | Value | Description |
|----------|-------|-------------|
| `invalid_id` | `0` | Sentinel for "not yet subscribed". Valid IDs start at 1. |

**String-based API:**

| Method | Signature | Description |
|--------|-----------|-------------|
| `subscribe` | `[[nodiscard]] SubscriptionId subscribe(const std::string& topic, Handler handler, int priority = 0)` | Subscribe to a topic. |
| `publish` | `void publish(const std::string& topic, const std::string& payload = {})` | Publish to all string subscribers on the topic. |

**Typed API:**

| Method | Signature | Description |
|--------|-----------|-------------|
| `subscribe_typed<T>` | `template <typename T> [[nodiscard]] SubscriptionId subscribe_typed(const std::string& topic, TypedHandler<T> handler, int priority = 0)` | Subscribe to typed events. |
| `publish_typed<T>` | `template <typename T> void publish_typed(const std::string& topic, const T& event)` | Publish a typed event. |

**Vetoable API:**

| Method | Signature | Description |
|--------|-----------|-------------|
| `subscribe_vetoable` | `[[nodiscard]] SubscriptionId subscribe_vetoable(const std::string& topic, VetoHandler handler, int priority = 0)` | Subscribe a veto handler (returns `true` to allow, `false` to veto). |
| `publish_vetoable` | `[[nodiscard]] bool publish_vetoable(const std::string& topic, const std::string& payload = {}, bool stop_on_veto = true)` | Publish with veto support. Returns `true` if event was NOT vetoed. |

**Common:**

| Method | Signature | Description |
|--------|-----------|-------------|
| `unsubscribe` | `bool unsubscribe(SubscriptionId id)` | Unsubscribe by ID. Works across all three APIs. |
| `clear` | `void clear()` | Remove all subscriptions. |
| `subscriber_count` | `[[nodiscard]] std::size_t subscriber_count(const std::string& topic) const` | Total subscribers on a topic. |
| `typed_subscriber_count` | `[[nodiscard]] std::size_t typed_subscriber_count(const std::string& topic) const` | Typed subscribers on a topic. |
| `vetoable_subscriber_count` | `[[nodiscard]] std::size_t vetoable_subscriber_count(const std::string& topic) const` | Vetoable subscribers on a topic. |
| `next_subscription_id` | `[[nodiscard]] SubscriptionId next_subscription_id() const` | The next ID that will be assigned. |

---

## Infrastructure

### SemVer
**Header:** `plugin_arch/SemVer.hpp`

**`struct SemVer`:**

| Field | Type | Default |
|-------|------|---------|
| `major` | `int` | `0` |
| `minor` | `int` | `0` |
| `patch` | `int` | `0` |

| Method | Signature | Description |
|--------|-----------|-------------|
| `operator<=>` | `constexpr auto operator<=>(const SemVer&) const = default` | Three-way comparison (C++20). |
| `parse` | `static SemVer parse(std::string_view s)` | Parse from `"1"`, `"1.2"`, or `"1.2.3"`. |
| `to_string` | `[[nodiscard]] std::string to_string() const` | Returns `"major.minor.patch"`. |

**`struct Dependency`:**

| Field | Type | Default |
|-------|------|---------|
| `type` | `std::string` | |
| `op` | `Op` | `Op::any` |
| `version` | `SemVer` | |

**`enum class Dependency::Op`:** `any`, `eq`, `ne`, `lt`, `le`, `gt`, `ge`, `caret`, `tilde`

| Method | Signature | Description |
|--------|-----------|-------------|
| `satisfied_by` | `[[nodiscard]] bool satisfied_by(const SemVer& v) const` | Check if version `v` satisfies this constraint. |
| `parse` | `static Dependency parse(std::string_view s)` | Parse from strings like `"logger"`, `"logger >= 2.0"`, `"logger ^1.2.3"`. |

Caret (`^`): `^1.2.3` matches `>=1.2.3, <2.0.0`; `^0.2.3` matches `>=0.2.3, <0.3.0`.
Tilde (`~`): `~1.2.3` matches `>=1.2.3, <1.3.0`.

---

### PluginError hierarchy
**Header:** `plugin_arch/PluginError.hpp`

```
std::runtime_error
  +-- PluginError              (base -- carries library path)
        +-- LoadError          (dlopen/LoadLibrary failed)
        +-- SymbolError        (dlsym/GetProcAddress failed)
        +-- AllocationError    (allocator returned nullptr)
        +-- InterfaceError     (dynamic_cast failed)
```

**`class PluginError` (base):**

| Method | Signature | Description |
|--------|-----------|-------------|
| *(constructor)* | `PluginError(std::string library_path, const std::string& message)` | |
| `library_path` | `[[nodiscard]] const std::string& library_path() const` | The library that caused the error. |

**`class LoadError`:**

| Method | Signature | Description |
|--------|-----------|-------------|
| `make` | `static LoadError make(std::string library_path, std::string platform_error)` | Factory. |
| `platform_error` | `[[nodiscard]] const std::string& platform_error() const` | Platform-specific error string. |

**`class SymbolError`:**

| Method | Signature | Description |
|--------|-----------|-------------|
| `make` | `static SymbolError make(std::string library_path, std::string symbol_name, std::string platform_error)` | Factory. |
| `symbol_name` | `[[nodiscard]] const std::string& symbol_name() const` | The symbol that was not found. |
| `platform_error` | `[[nodiscard]] const std::string& platform_error() const` | Platform-specific error string. |

**`class AllocationError`:**

| Method | Signature | Description |
|--------|-----------|-------------|
| `make` | `static AllocationError make(std::string library_path)` | Factory. |

**`class InterfaceError`:**

| Method | Signature | Description |
|--------|-----------|-------------|
| `make` | `static InterfaceError make(std::string library_path, std::string requested_type)` | Factory. |
| `requested_type` | `[[nodiscard]] const std::string& requested_type() const` | The type name that `dynamic_cast` failed for. |

---

### ThreadSafe\<T\>
**Header:** `plugin_arch/ThreadSafe.hpp`

Thread-safe wrappers. Primary template is not defined -- only explicit specializations exist.

**Available specializations:**

| Specialization | Mutex type | Notes |
|----------------|-----------|-------|
| `ThreadSafe<ServiceLocator>` | `shared_mutex` | Mirrors all `ServiceLocator` methods. |
| `ThreadSafe<PluginRegistry>` | `shared_mutex` | Mirrors all `PluginRegistry` methods. |
| `ThreadSafe<HotPluginLoader<T>>` | `shared_mutex` | Partial specialization. |
| `ThreadSafe<EventBus>` | `recursive_mutex` | Allows re-entrant subscribe during publish. |
| `ThreadSafe<PluginManager>` | `shared_mutex` | Mirrors all `PluginManager` methods. |

Each specialization exposes the same public API as the wrapped type. Read operations take shared locks; write operations take unique locks.

---

### MockPluginLoader\<T\>
**Header:** `plugin_arch/MockPluginLoader.hpp`

Drop-in replacement for `PluginLoader<T>` for testing without the filesystem.

| Method | Signature | Description |
|--------|-----------|-------------|
| *(constructor)* | `explicit MockPluginLoader(std::shared_ptr<T> instance, std::string path = "mock")` | Wraps a pre-existing instance. |
| `get_instance` | `[[nodiscard]] std::shared_ptr<T> get_instance()` | Returns the stored instance. |
| `path` | `[[nodiscard]] const std::string& path() const` | Returns the mock path. |

### MockHotPluginLoader\<T\>

Drop-in replacement for `HotPluginLoader<T>` for testing.

| Method | Signature | Description |
|--------|-----------|-------------|
| *(constructor)* | `explicit MockHotPluginLoader(std::shared_ptr<T> instance, std::string library_path = "mock")` | Wraps a pre-existing instance. |
| `get_instance` | `[[nodiscard]] std::shared_ptr<T> get_instance() const` | Returns the stored instance. |
| `set_instance` | `void set_instance(std::shared_ptr<T> instance)` | Simulate a reload by swapping in a new instance. |
| `check_and_reload` | `[[nodiscard]] bool check_and_reload()` | Always returns `false`. |
| `reload` | `void reload()` | No-op. |
| `library_path` | `[[nodiscard]] const std::string& library_path() const` | Returns the mock path. |

---

## Platform

### platform/visibility.hpp
**Header:** `plugin_arch/platform/visibility.hpp`

| Macro | Description |
|-------|-------------|
| `EXPORTED` | `__declspec(dllexport)` on Windows, `__attribute__((visibility("default")))` on GCC/Clang. |
| `NOT_EXPORTED` | `__attribute__((visibility("hidden")))` on GCC/Clang, empty on Windows. |

### platform/extern_c.hpp
**Header:** `plugin_arch/platform/extern_c.hpp`

| Macro | Description |
|-------|-------------|
| `EXPORT_C` | `extern "C"` in C++, empty in C. |

### platform/shared_lib.hpp
**Header:** `plugin_arch/platform/shared_lib.hpp`
**Namespace:** `plugin_arch::platform`

| Function | Signature | Description |
|----------|-----------|-------------|
| `shared_lib_extension` | `[[nodiscard]] constexpr std::string_view shared_lib_extension()` | Returns `".dll"`, `".dylib"`, or `".so"`. |
| `is_shared_library` | `[[nodiscard]] inline bool is_shared_library(const std::filesystem::path& path)` | Checks platform extension. |
| `find_plugin` | `[[nodiscard]] inline std::filesystem::path find_plugin(const std::filesystem::path& dir, const std::string& name)` | Finds first shared library whose filename contains `name`. |

### platform/clr_helpers.hpp
**Header:** `plugin_arch/platform/clr_helpers.hpp`
**Namespace:** `plugin_arch::platform`
**Condition:** Only active when compiled with MSVC `/clr`.

**`class ManagedHandle<T>`** -- RAII wrapper around `msclr::gcroot<T^>`.

| Method | Signature | Description |
|--------|-----------|-------------|
| *(constructor)* | `ManagedHandle()` / `explicit ManagedHandle(T^ obj)` | Default or wrap a managed object. |
| `reset` | `void reset(T^ obj)` | Replace the held object. |
| `get` | `T^ get()` | Access the managed object. |
| `operator->` | `T^ operator->()` | Pointer-style access. |
| `operator bool` | `explicit operator bool() const` | Check if non-null. |

**Free functions:**

| Function | Signature | Description |
|----------|-----------|-------------|
| `marshal_to_native` | `inline std::string marshal_to_native(System::String^ str)` | Managed `String^` to `std::string`. |
| `marshal_to_managed` | `inline System::String^ marshal_to_managed(const char* str)` | C string to managed `String^`. |
