# Example Problem Statements

Each example solves a specific real-world problem. This document explains *when* and *why* you would reach for each pattern.

---

## 1. Core Loading Patterns

### calculator
**Problem:** You have a host application that needs to load one or more implementations of the same interface at runtime from shared libraries, without knowing which implementations exist at compile time. Two plugins (`BasicCalc`, `ScientificCalc`) implement `ICalculator` and the host exercises both through the same interface.

**When you need this:**
- Building an application where users drop in their own calculation engines
- Supporting multiple backend implementations selected at runtime
- Comparing outputs from different plugin implementations side by side

**Key library features used:** `PluginLoader<T>`, `IPlugin`, `platform::shared_lib_extension()`

---

### libA_example
**Problem:** You have an existing library and want to wrap it behind a plugin interface so a host can load it at runtime with zero compile-time coupling. The host knows only the abstract interface, never the concrete implementation.

**When you need this:**
- Wrapping a team's internal library as a plugin without modifying its public API
- Decoupling a monolith by extracting modules into loadable plugins
- Allowing hot-swapping of a processing backend

**Key library features used:** `PluginLoader<T>`, `IPlugin`, `platform::find_plugin()`

---

### libB_example
**Problem:** You have a third-party library that does not conform to the plugin interface. You need an adapter layer that wraps it behind `IPlugin` so the host can load it identically to any other plugin.

**When you need this:**
- Integrating a vendor SDK behind a plugin interface
- Providing a stable API surface while the underlying library changes
- Wrapping a C library with a C++ plugin adapter

**Key library features used:** `PluginLoader<T>`, `IPlugin`, `platform::shared_lib_extension()`

---

### dynamic_plugin_demo
**Problem:** You need to load a shared library and call its exported C functions by name, without any shared interface header. The plugin self-describes its API via a `PluginDescriptor`, and the host discovers and resolves functions dynamically.

**When you need this:**
- Loading plugins written in C or other languages that export a flat C ABI
- Building a generic plugin host that discovers APIs at runtime via introspection
- Probing a library for optional functions before calling them
- Supporting legacy symbol conventions alongside descriptor-based APIs

**Key library features used:** `DynamicLibrary`, `PluginDescriptor`, `DynamicLibrary::resolve<T>()`, `DynamicLibrary::has()`

---

### versioned_calculator
**Problem:** Your plugin interface needs to evolve (add methods) without breaking existing plugins. `ICalculatorV2` extends `ICalculator` -- old plugins continue to work unchanged. The host uses `dynamic_cast` to detect which plugins support the extended interface.

**When you need this:**
- Evolving a plugin API across major versions without a flag-day migration
- Allowing old and new plugins to coexist in the same host process
- Adding optional capabilities that only some plugins support

**Key library features used:** `PluginLoader<T>`, interface inheritance, `dynamic_cast` for capability detection

---

### hot_reload_demo
**Problem:** You need to update a plugin's code while the host application is running, without restarting. The host polls the shared library file for modification-time changes and transparently swaps to the new version.

**When you need this:**
- Rapid iteration during development (edit, recompile, see results immediately)
- Updating production plugins without downtime
- Live-patching behavior in a long-running server process

**Key library features used:** `HotPluginLoader<T>`, `check_and_reload()`, `get_instance()`

---

## 2. Discovery & Lifecycle

### registry_demo
**Problem:** Instead of hardcoding plugin filenames, you need automatic discovery: scan a directory, introspect every shared library, and build a queryable catalog of plugins with their metadata.

**When you need this:**
- Building an application with a "plugins" folder that users populate freely
- Querying "give me all plugins of type X" without knowing filenames
- Providing an admin UI that lists all available plugins
- Gracefully handling libraries that are not valid plugins (error reporting)

**Key library features used:** `PluginRegistry`, `PluginEntry`, `scan()`, `get()`, `get_all()`, `errors()`

---

### lifecycle_demo
**Problem:** Plugins need setup and teardown steps beyond construction/destruction -- opening database connections on init, closing them on shutdown. The host detects these opt-in capabilities via `dynamic_cast`.

**When you need this:**
- Plugins that open network connections, file handles, or GPU contexts on startup
- Injecting configuration before a plugin begins operating
- Ensuring clean shutdown ordering (connections closed before destructors run)

**Key library features used:** `ILifecycleAware`, `IConfigurable`, `PluginConfig`, `dynamic_cast` for mixin detection

---

### service_locator_demo
**Problem:** Plugins need to communicate with each other without compile-time knowledge of one another. A `ServiceLocator` acts as a runtime registry where the host registers plugin instances by type, and dependent plugins discover them via `IServiceAware`.

**When you need this:**
- A report generator that needs a stats engine and a text formatter, loaded as separate plugins
- Decoupling plugin-to-plugin dependencies so they can be loaded in any order
- Injecting shared services (logging, config, caching) into multiple plugins

**Key library features used:** `ServiceLocator`, `IServiceAware`, `PluginLoader<T>`, `dynamic_cast`

---

## 3. Orchestration (PluginManager)

### managed_demo
**Problem:** When plugins have inter-dependencies (Aggregator depends on Processor, which depends on Logger), the host must load them in the correct topological order, wire services, apply configuration, and shut them down in reverse. `PluginManager` automates all of this.

**When you need this:**
- Loading a graph of interdependent plugins from a directory in one call
- Automatic dependency resolution and topological ordering
- Centralized per-plugin configuration via a config map
- Clean reverse-order shutdown of all plugins

**Key library features used:** `PluginManager`, `load_all()`, `ConfigMap`, `IDependencyAware`, `IServiceAware`, `IConfigurable`

---

### features_demo
**Problem:** A production plugin system needs health monitoring, enable/disable toggling, event priority dispatch, vetoable events, and conflict detection. This example exercises all five PluginManager B-tier features in a single demo.

**When you need this:**
- Monitoring database connection health and alerting on failures
- Temporarily disabling a cache plugin without unloading it
- Ensuring event handlers fire in priority order (cache before database)
- Preventing two conflicting plugins from loading simultaneously
- Allowing a guard handler to veto dangerous operations

**Key library features used:** `PluginManager`, `IHealthAware`, `check_health()`, `disable()`/`enable()`, `EventBus` (priority, vetoable), `IConflictAware`

---

### batch_loading
**Problem:** You have a manifest of plugins with declared dependencies and need to load them in the correct order even though the manifest is unordered. This shows host-side topological sorting before feeding entries to `PluginManager`.

**When you need this:**
- Loading plugins from a JSON/YAML manifest where order is not guaranteed
- Validating dependency completeness before loading anything
- Detecting circular dependencies early with clear error messages
- Controlling exact load order in environments where directory scanning is inappropriate

**Key library features used:** `PluginManager`, `add_plugin()`, `IDependencyAware`, `ILifecycleAware`, `dependency_graph()`

---

## 4. Interop

### dotnet_bridge
**Problem:** You need to call into a plugin backed by a different runtime (.NET, Python, Lua) via a C ABI. This demonstrates scalar calls, batch array operations, string marshaling with explicit free, and performance benchmarking across the FFI boundary.

**When you need this:**
- Hosting .NET/Python/Lua plugins from a C++ application via C ABI
- Passing arrays across the FFI boundary for batch processing
- Marshaling strings with explicit allocation/free protocols
- Benchmarking FFI call overhead

**Key library features used:** `DynamicLibrary`, `PluginDescriptor`, `resolve<T>()`, `has()`, C ABI conventions

---

## 5. Diagnostics & Monitoring

### crash_diagnostic
**Problem:** A black-box application is crashing when loading your shared library. This diagnostic tool tests each loading stage in isolation -- dlopen, symbol resolution, allocator call, deallocator call -- and reports exactly which step fails.

**When you need this:**
- Debugging why a plugin fails to load in a third-party host application
- Verifying that a shared library exports the expected symbols before deployment
- Diagnosing missing transitive dependencies (wrong rpath, missing .dylib)
- Pre-flight validation of a plugin binary in CI/CD

**Key library features used:** Raw `dlopen`/`dlsym`/`dlclose` (platform-level), symbol probing

---

### diagnostic_dump
**Problem:** In production, you need a single-call diagnostic snapshot showing all loaded plugins, their metadata, dependency graph, health status, and enabled/disabled state. The plugin system equivalent of a `/health` endpoint.

**When you need this:**
- Building an admin dashboard or status page for a plugin-based application
- Debugging "which plugins are loaded and are any unhealthy?" in production
- Generating support bundles that capture the full plugin system state

**Key library features used:** `PluginManager`, `loaded_plugins()`, `dependency_graph()`, `check_health()`, `IPluginMetadata`, `IHealthAware`

---

### metrics_observer
**Problem:** You need to collect operational metrics (load counts, timing, enable/disable cycles) from the plugin system without modifying the library or any plugin code. `PluginObserver` provides lifecycle hooks the host implements.

**When you need this:**
- Tracking how many times each plugin has been loaded/reloaded in a long-running process
- Measuring plugin load times for performance monitoring
- Feeding plugin lifecycle events into a metrics pipeline (Prometheus, StatsD, etc.)

**Key library features used:** `PluginObserver`, `PluginManager::add_observer()`, `unload()`, `disable()`/`enable()`

---

### dot_graph
**Problem:** You need to visualize the dependency graph of your plugin system as a diagram. Converts dependency data into Graphviz DOT format for rendering to PNG/SVG.

**When you need this:**
- Generating architecture diagrams for documentation
- Debugging dependency chains ("why does unloading X cascade to Y?")
- Visualizing which plugins are disabled (grayed-out nodes)

**Key library features used:** `PluginManager`, `loaded_plugins()`, `dependency_graph()`, `IDependencyAware`

---

### event_replay
**Problem:** You need a flight-recorder for your plugin system: capture all lifecycle events and bus messages in a ring buffer, then replay them on demand for post-mortem debugging.

**When you need this:**
- Post-mortem debugging ("what happened in the last 60 seconds before the crash?")
- Reproducing intermittent issues by replaying event sequences
- Auditing plugin lifecycle operations with timestamps
- Keeping a bounded event history without unbounded memory growth

**Key library features used:** `PluginObserver`, `EventBus`, `subscribe()`, ring buffer pattern (host-side)

---

## 6. Host-Level Patterns

### config_hot_reload
**Problem:** You need to change a plugin's configuration at runtime without reloading its shared library. The pattern: disable (triggers shutdown), supply new config, re-enable (triggers configure + init). Also shows `IConfigSchema` for self-documenting configuration.

**When you need this:**
- Changing log levels in production without restarting
- Rotating credentials or connection strings on the fly
- A/B testing different plugin configurations
- Plugins that declare their own config schemas for validation and defaults

**Key library features used:** `PluginManager`, `disable()`/`enable()` with `ConfigMap`, `IConfigurable`, `IConfigSchema`, `ILifecycleAware`

---

### capability_negotiation
**Problem:** Before starting work, the host must verify that all required capabilities (storage, SQL, cache) are provided by at least one loaded plugin. If a capability is missing, the host can degrade gracefully or refuse to start.

**When you need this:**
- A microservice that refuses to start without a storage backend
- Graceful degradation when optional capabilities (auth, search) are missing
- Disabling all plugins in a capability group for maintenance
- Feature-flag-style logic based on available plugin capabilities

**Key library features used:** `PluginManager`, `plugins_with_capability()`, `disable_group()`/`enable_group()`, `IPluginMetadata`

---

### lazy_init
**Problem:** Loading all plugins at startup is wasteful when some may never be used. This wraps `ServiceLocator` with a lazy layer that registers factories upfront but defers construction until first use.

**When you need this:**
- Reducing startup time by deferring rarely-used plugins
- Memory-constrained environments where you only load what is needed
- Building a service mesh where plugins are instantiated on first request

**Key library features used:** `PluginManager`, `ServiceLocator`, `add_plugin()`, `ILifecycleAware`, factory pattern (host-side)

---

### wildcard_routing
**Problem:** The `EventBus` supports exact-topic subscriptions, but real applications need pattern-based routing (e.g., subscribe to all `order.*` events) and event fan-out (aggregate all events into an audit log). This implements both on top of the existing API.

**When you need this:**
- Subscribing to all events in a domain with one handler
- Building an audit log that captures every event across the system
- Implementing event dispatchers that route from source topics to summary topics

**Key library features used:** `EventBus`, `subscribe()`/`unsubscribe()`, `publish()`, host-side prefix matching
