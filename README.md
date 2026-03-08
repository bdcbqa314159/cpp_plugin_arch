# cpp_plugin_arch

A C++20 header-only plugin architecture library: runtime polymorphism across shared library boundaries. The host loads plugins at runtime knowing only the interface — no recompilation needed when plugins are added or swapped.

## Project structure

```
cpp_plugin_arch/
├── plugin_arch/                            # Header-only library (31 headers)
│   ├── plugin_arch.hpp                     # Umbrella include
│   │
│   │ ── Core ──
│   ├── IPlugin.hpp                         # Base interface (name, version, type)
│   ├── PluginLoader.hpp                    # RAII cross-platform loader (dlopen/LoadLibrary)
│   ├── PluginExport.hpp                    # REGISTER_PLUGIN() macro
│   ├── DynamicLibrary.hpp                  # Non-templated RAII loader (no type assumptions)
│   ├── PluginDescriptor.hpp                # POD descriptor + REGISTER_DYNAMIC_PLUGIN() macro
│   ├── DynamicPluginAdapter.hpp            # Wraps DynamicLibrary as IPlugin
│   │
│   │ ── Discovery & orchestration ──
│   ├── PluginRegistry.hpp                  # Directory scanner + metadata index
│   ├── PluginManager.hpp                   # Full orchestration (deps, config, lifecycle, wiring)
│   ├── HotPluginLoader.hpp                 # Hot-reload wrapper (polling + copy-on-swap)
│   │
│   │ ── Opt-in mixins (detected via dynamic_cast) ──
│   ├── ILifecycleAware.hpp                 # on_init() / on_shutdown() hooks
│   ├── IConfigurable.hpp                   # Key-value configuration
│   ├── IConfigSchema.hpp                   # Config schema with defaults + required keys
│   ├── IDependencyAware.hpp                # Declare dependency types for topo sort
│   ├── IServiceAware.hpp                   # Receive ServiceLocator injection
│   ├── IEventAware.hpp                     # Receive EventBus injection
│   ├── IHealthAware.hpp                    # Health check reporting
│   ├── IConflictAware.hpp                  # Declare conflicting plugin types
│   ├── IPluginMetadata.hpp                 # Author, description, license, capabilities
│   ├── ISerializable.hpp                   # State save/restore
│   ├── PluginObserver.hpp                  # Lifecycle event callbacks (load/unload/enable/disable)
│   │
│   │ ── Communication ──
│   ├── ServiceLocator.hpp                  # Plugin-to-plugin service discovery
│   ├── EventBus.hpp                        # Pub-sub (string, typed, vetoable, prioritized)
│   │
│   │ ── Infrastructure ──
│   ├── SemVer.hpp                          # Semantic versioning parser + comparator
│   ├── PluginError.hpp                     # Error hierarchy (LoadError, SymbolError, etc.)
│   ├── ThreadSafe.hpp                      # Thread-safe wrappers (shared_mutex / recursive_mutex)
│   ├── MockPluginLoader.hpp                # Test doubles for PluginLoader / HotPluginLoader
│   └── platform/
│       ├── visibility.hpp                  # Symbol visibility macros
│       ├── extern_c.hpp                    # extern "C" wrapper macro
│       ├── shared_lib.hpp                  # Platform extension helpers
│       └── clr_helpers.hpp                 # .NET/CLR interop helpers
│
├── examples/                               # 22 examples (see below)
├── tests/                                  # Catch2 test suite (161 tests, 404 assertions)
└── CMakeLists.txt
```

## How it works

1. **Define an interface** — a pure virtual class extending `plugin_arch::IPlugin`
2. **Implement plugins** — concrete classes that implement the interface, each compiled to a `.so`/`.dylib`/`.dll`
3. **Register plugins** — one line: `REGISTER_PLUGIN(MyClass)` generates the `extern "C"` factory
4. **Load from the host** — `PluginLoader<IMyInterface>` opens the library, resolves symbols, returns a `shared_ptr`

The host never sees plugin source code. It only needs the interface header and the compiled binary.

### Opt-in mixins

Plugins gain extra capabilities by inheriting additional interfaces. The host (or `PluginManager`) detects them via `dynamic_cast` — plugins that don't need a feature simply don't inherit it:

| Mixin | Purpose |
|-------|---------|
| `ILifecycleAware` | `on_init()` / `on_shutdown()` hooks |
| `IConfigurable` | Accept key-value config via `configure()` |
| `IConfigSchema` | Declare config schema with defaults and required keys |
| `IDependencyAware` | Declare dependency types for topological sort |
| `IServiceAware` | Receive `ServiceLocator` injection for plugin-to-plugin calls |
| `IEventAware` | Receive `EventBus` injection for pub-sub messaging |
| `IHealthAware` | Report health status via `is_healthy()` / `health_status()` |
| `IConflictAware` | Declare conflicting plugin types (prevents co-loading) |
| `IPluginMetadata` | Expose author, description, license, capabilities |
| `ISerializable` | Save/restore plugin state |
| `PluginObserver` | Receive callbacks on lifecycle events (load/unload/enable/disable) |

### PluginManager orchestration

`PluginManager` handles the full plugin lifecycle automatically:

1. **Conflict check** — `IConflictAware` plugins declare types they can't coexist with
2. **Topological sort** — `IDependencyAware` plugins are loaded after their dependencies
3. **Version check** — dependencies can specify semver constraints (e.g., `"database>=2.0.0"`)
4. **Configuration** — `IConfigurable` plugins receive config; `IConfigSchema` validates and fills defaults
5. **Service wiring** — `IServiceAware` plugins receive the `ServiceLocator`; `IEventAware` receive the `EventBus`
6. **Lifecycle** — `ILifecycleAware` plugins get `on_init()` after setup and `on_shutdown()` before teardown
7. **Observer notifications** — registered `PluginObserver` instances receive all lifecycle events

## Examples

### Core loading patterns

#### Calculator — two plugins, same interface

The simplest case. Two plugins (`BasicCalc`, `ScientificCalc`) implement `ICalculator`. The host loads both and exercises them through the same interface.

```bash
./build/bin/host
```

#### libA — you have the source code

You have access to the library source. Add `: public ITextFormatter` to the class, add `REGISTER_PLUGIN()` at the bottom of the `.cpp`, build as SHARED. Done.

```bash
./build/bin/libA_host
```

#### libB — you only have the artifact + headers

You have a pre-built `.dylib` and its header, but no source code. Write a thin **adapter plugin** that wraps the vendor library and implements the plugin interface.

```bash
./build/bin/libB_host
```

#### Dynamic plugin — no headers at all

Uses `DynamicLibrary` and `PluginDescriptor` to load a plugin with zero interface headers. The plugin exports pure `extern "C"` functions and a `plugin_describe()` descriptor. The host discovers available functions at runtime, resolves them by name, and calls them — no vtables, no `IPlugin`.

```bash
./build/bin/dynamic_plugin_demo
```

### Discovery & lifecycle

#### Plugin registry — discover plugins by metadata

Uses `PluginRegistry` to scan a directory and index all plugins by their `IPlugin` metadata (name, version, type). No filename conventions needed — the registry probes each shared library and queries its metadata directly.

```bash
./build/bin/registry_demo
```

#### Lifecycle demo — opt-in init/shutdown hooks

Uses `ILifecycleAware` to give plugins post-construction setup and pre-destruction teardown hooks. The host detects the mixin via `dynamic_cast`. Plugins that don't need lifecycle hooks simply don't inherit it.

```bash
./build/bin/lifecycle_demo
```

#### Versioned calculator — interface versioning

Demonstrates how to evolve an interface without breaking existing plugins. `ICalculatorV2` extends `ICalculator` with trig and log methods — the original `ICalculator` is frozen. Existing v1 plugins continue to work unchanged. The host uses `dynamic_cast<ICalculatorV2*>` to detect which plugins support the extended interface.

```bash
./build/bin/versioned_calculator_host
```

#### Hot-reload — swap plugins at runtime

Uses `HotPluginLoader<T>` to detect when a plugin's `.dylib` has been rebuilt and reload it without restarting the host. The old library stays loaded until all `shared_ptr`s to the old instance are released.

```bash
# Terminal 1: run the host
./build/bin/hot_reload_demo

# Terminal 2: edit the greeting in greeter.cpp, then rebuild
cmake --build build

# Terminal 1 shows: [reloaded] + new greeting
```

### Orchestration (PluginManager)

#### Managed demo — full PluginManager end-to-end

Three shared-library plugins (Logger, Processor, Aggregator) with dependencies. `PluginManager` discovers them from disk, topologically sorts by dependencies, validates configs against schemas, wires services, and manages the full lifecycle.

```bash
./build/bin/managed_demo
```

#### Features demo — health, conflicts, events, enable/disable

Demonstrates `PluginManager` B-tier features using in-process plugins: health checks (`IHealthAware`), enable/disable toggling, event priority (`EventBus`), conflict detection (`IConflictAware`), and vetoable events.

```bash
./build/bin/features_demo
```

### Interop

#### .NET bridge — C++/CLI interop

Demonstrates loading a .NET assembly as a plugin via `DynamicLibrary` and a C++/CLI bridge layer. Requires MSVC with `/clr` support (Windows only).

```bash
./build/bin/dotnet_bridge   # Windows/MSVC only
```

### Diagnostics & monitoring

#### Crash diagnostic — debug loading failures

A diagnostic tool that loads any `.dylib`/`.so`/`.dll` with full error reporting at every stage. Includes three intentionally broken plugins:

| Plugin | Failure | Diagnostic output |
|---|---|---|
| `libmissing_symbols.dylib` | No `extern "C"` — symbols are mangled | Step 2: symbol not found |
| `libcrash_in_constructor.dylib` | Constructor throws | Step 3: exception message |
| `libcrash_on_load.dylib` | Static initializer crashes inside `dlopen` | Step 1: process aborts |

```bash
./build/bin/crash_diag /path/to/your/library.dylib
```

#### Diagnostic dump — full system report

Builds a comprehensive system snapshot from `loaded_plugins()`, `check_health()`, `dependency_graph()`, and `IPluginMetadata`. Shows plugin inventory, dependency graph, health status, and metadata in one call.

```bash
./build/bin/diagnostic_dump
```

#### Metrics observer — operational metrics collection

Implements `PluginObserver` to track load/unload/enable/disable/reload counts per plugin. Includes a `timed_add_plugin()` wrapper that measures load time with `steady_clock`.

```bash
./build/bin/metrics_observer
```

### Host-level patterns

These examples show patterns built on top of the library API — no library modifications needed.

#### Config hot-reload — reconfigure without reloading

Reconfigures a plugin at runtime via the `disable()` → `enable(name, new_config)` pattern. The plugin gets `on_shutdown()`, then fresh `configure()` + `on_init()` with the new config. Uses `IConfigSchema` for validation.

```bash
./build/bin/config_hot_reload
```

#### Capability negotiation — check prerequisites before loading

Uses `plugins_with_capability()` to verify that required capabilities (storage, SQL, cache, etc.) are available before proceeding. Demonstrates `disable_group()` / `enable_group()` for bulk operations on plugins sharing a capability.

```bash
./build/bin/capability_negotiation
```

#### DOT graph export — visualize dependencies

Converts `loaded_plugins()` dependency data into Graphviz DOT format. Pipe the output to `dot -Tpng -o deps.png` for a visual dependency graph.

```bash
./build/bin/dot_graph | dot -Tpng -o deps.png
```

#### Lazy initialization — load on first use

Wraps `ServiceLocator` with a `LazyServiceLocator` that defers plugin loading until a service is first requested. Factories are registered upfront; `add_plugin()` is called on cache miss.

```bash
./build/bin/lazy_init
```

#### Wildcard event routing — prefix subscriptions + dispatcher

Subscribes to multiple `EventBus` topics by prefix convention (e.g., all `order.*` topics). Includes an `EventDispatcher` that fans out all watched topics into a single audit log topic.

```bash
./build/bin/wildcard_routing
```

#### Event replay — lifecycle history ring buffer

Implements `PluginObserver` with a fixed-size ring buffer that records timestamped lifecycle and bus events. Replay the full history on demand for debugging.

```bash
./build/bin/event_replay
```

#### Batch loading — manifest-driven topo sort

Loads a set of plugins from a manifest (name, type, version, deps, factory). Host-side topological sort resolves dependency order, then calls `add_plugin()` sequentially. Detects circular dependencies and missing providers.

```bash
./build/bin/batch_loading
```

## Writing a plugin

```cpp
// my_plugin.hpp
#include "ICalculator.hpp"

class MyPlugin : public examples::ICalculator {
 public:
  std::string name() const override { return "MyPlugin"; }
  std::string version() const override { return "1.0.0"; }
  std::string type() const override { return "calculator"; }

  double add(double a, double b) override { return a + b; }
  // ... implement the rest
};
```

```cpp
// my_plugin.cpp
#include "my_plugin.hpp"
#include "PluginExport.hpp"

// This is all you need — generates allocator() and deallocator()
REGISTER_PLUGIN(MyPlugin)
```

## Loading plugins from the host

```cpp
#include "ICalculator.hpp"
#include "PluginLoader.hpp"

plugin_arch::PluginLoader<examples::ICalculator> loader("./libmy_plugin.dylib");
auto plugin = loader.get_instance();

std::cout << plugin->name() << "\n";      // "MyPlugin"
std::cout << plugin->add(10, 3) << "\n";  // 13
```

## Build

```bash
cmake -B build -G Ninja
cmake --build build
```

## Docker (Linux on macOS)

```bash
docker build -t cpp_plugin_arch .
docker run --rm -v $(pwd):/workspace cpp_plugin_arch
```
