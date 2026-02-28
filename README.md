# cpp_plugin_arch

A C++20 plugin architecture library: runtime polymorphism across shared library boundaries. The host loads plugins at runtime knowing only the interface — no recompilation needed when plugins are added or swapped.

## Project structure

```
cpp_plugin_arch/
├── plugin_arch/                            # Reusable header-only library
│   ├── plugin_arch                         # Umbrella include
│   ├── IPlugin.hpp                         # Base interface (name, version, type)
│   ├── ILifecycleAware.hpp                 # Opt-in lifecycle hooks (on_init/on_shutdown)
│   ├── PluginLoader.hpp                    # RAII cross-platform loader (dlopen/LoadLibrary)
│   ├── HotPluginLoader.hpp                 # Hot-reload wrapper (polling + copy-on-swap)
│   ├── PluginRegistry.hpp                  # Directory scanner + metadata index
│   ├── ServiceLocator.hpp                  # Plugin-to-plugin communication
│   ├── PluginFactory.hpp                   # REGISTER_PLUGIN() macro
│   ├── DynamicLibrary.hpp                  # Non-templated RAII loader (no type assumptions)
│   ├── PluginDescriptor.hpp                # POD descriptor + REGISTER_DYNAMIC_PLUGIN() macro
│   └── platform/
│       ├── exported.hpp                    # Symbol visibility macros
│       ├── abi.hpp                         # extern "C" wrapper macro
│       └── shared_lib.hpp                  # Platform extension helpers
│
├── examples/
│   ├── calculator/                         # Two plugins, same interface (polymorphism)
│   │   ├── interfaces/ICalculator.hpp
│   │   ├── plugins/basic_calc/             # Arithmetic only
│   │   ├── plugins/scientific_calc/        # Adds pow, sqrt
│   │   └── host/main.cpp
│   │
│   ├── libA_example/                       # You have the source code → plug it in
│   │   ├── interfaces/ITextFormatter.hpp
│   │   ├── libA/                           # Existing library, made into a plugin
│   │   └── host/main.cpp
│   │
│   ├── libB_example/                       # You only have .dylib + header → adapter
│   │   ├── interfaces/IStatsEngine.hpp
│   │   ├── prebuilt/include/libB.hpp       # Vendor header (all you have)
│   │   ├── adapter/                        # Thin wrapper that pluginifies libB
│   │   └── host/main.cpp
│   │
│   ├── registry_demo/                      # Discover plugins by metadata (registry)
│   │   └── host/main.cpp
│   │
│   ├── service_locator_demo/               # Plugin-to-plugin communication
│   │   ├── interfaces/IReportGenerator.hpp
│   │   ├── report_generator/               # Uses locator to find other plugins
│   │   └── host/main.cpp
│   │
│   ├── versioned_calculator/               # Interface versioning (v1 + v2 coexist)
│   │   ├── interfaces/ICalculatorV2.hpp    # Extends ICalculator with trig/log
│   │   ├── plugins/trig_calc/              # Implements ICalculatorV2
│   │   └── host/main.cpp                   # Loads v1 and v2 plugins together
│   │
│   ├── hot_reload_demo/                    # Swap plugins at runtime (hot-reload)
│   │   ├── interfaces/IGreeter.hpp
│   │   ├── plugins/greeter/                # Edit this, rebuild, see live reload
│   │   └── host/main.cpp
│   │
│   ├── dynamic_plugin_demo/                # No headers — pure C ABI discovery
│   │   ├── plugins/math_functions/         # Exports C functions + descriptor
│   │   └── host/main.cpp                  # Discovers and calls functions by name
│   │
│   ├── lifecycle_demo/                     # Opt-in init/shutdown hooks
│   │   ├── interfaces/IWorker.hpp          # Worker interface (process items)
│   │   ├── plugins/counting_worker/        # Implements ILifecycleAware
│   │   └── host/main.cpp                  # Detects mixin, calls hooks
│   │
│   └── crash_diagnostic/                   # Debug why a library crashes on load
│       ├── bad_plugins/                    # Intentionally broken plugins
│       └── host/main.cpp                   # Diagnostic loader (crash_diag)
```

## How it works

1. **Define an interface** — a pure virtual class extending `plugin_arch::IPlugin`
2. **Implement plugins** — concrete classes that implement the interface, each compiled to a `.so`/`.dylib`/`.dll`
3. **Register plugins** — one line: `REGISTER_PLUGIN(MyClass)` generates the `extern "C"` factory
4. **Load from the host** — `PluginLoader<IMyInterface>` opens the library, resolves symbols, returns a `shared_ptr`

The host never sees plugin source code. It only needs the interface header and the compiled binary.

## Examples

### Calculator — two plugins, same interface

The simplest case. Two plugins (`BasicCalc`, `ScientificCalc`) implement `ICalculator`. The host loads both and exercises them through the same interface.

```bash
./build/bin/host
```

### libA — you have the source code

You have access to the library source. Add `: public ITextFormatter` to the class, add `REGISTER_PLUGIN()` at the bottom of the `.cpp`, build as SHARED. Done.

```bash
./build/bin/libA_host
```

### libB — you only have the artifact + headers

You have a pre-built `.dylib` and its header, but no source code. Write a thin **adapter plugin** that wraps the vendor library and implements the plugin interface.

```bash
./build/bin/libB_host
```

### Plugin registry — discover plugins by metadata

Uses `PluginRegistry` to scan a directory and index all plugins by their `IPlugin` metadata (name, version, type). No filename conventions needed — the registry probes each shared library and queries its metadata directly.

```bash
./build/bin/registry_demo
```

The demo discovers all plugins in the build directory, lists them grouped by type, queries by type (`"calculator"`) and by name (`"BasicCalc"`), and reports any libraries that failed to load.

### Service locator — plugin-to-plugin communication

Uses `ServiceLocator` to let plugins discover and call each other at runtime. A report generator plugin queries the locator for a stats engine and a text formatter — no direct dependencies between plugins.

```bash
./build/bin/service_locator_demo
```

The host loads three plugins, registers the stats engine and text formatter as services, injects the locator into the report generator via the `IServiceAware` mixin, and the report generator produces output using the other two plugins.

### Versioned calculator — interface versioning

Demonstrates how to evolve an interface without breaking existing plugins. `ICalculatorV2` extends `ICalculator` with trig and log methods — but the original `ICalculator` is frozen. Existing v1 plugins (`BasicCalc`, `ScientificCalc`) continue to work unchanged.

```bash
./build/bin/versioned_calculator_host
```

The host loads all three calculator plugins as `ICalculator` (v1), exercises the common methods, then uses `dynamic_cast<ICalculatorV2*>` to detect which plugins support the extended interface. Only `TrigCalc` responds to the v2 probe — the other two safely return nullptr.

### Hot-reload — swap plugins at runtime

Uses `HotPluginLoader<T>` to detect when a plugin's `.dylib` has been rebuilt and reload it without restarting the host. The old library stays loaded until all `shared_ptr`s to the old instance are released, preventing crashes from dangling vtable pointers.

```bash
# Terminal 1: run the host
./build/bin/hot_reload_demo

# Terminal 2: edit the greeting in greeter.cpp, then rebuild
cmake --build build

# Terminal 1 shows: [reloaded] + new greeting
```

### Dynamic plugin — no headers at all

Uses `DynamicLibrary` and `PluginDescriptor` to load a plugin with zero interface headers. The plugin exports pure `extern "C"` functions and a `plugin_describe()` descriptor. The host discovers available functions at runtime, resolves them by name, and calls them — no vtables, no `IPlugin`, no `allocator`/`deallocator`.

```bash
./build/bin/dynamic_plugin_demo
```

The demo loads `libmath_functions.dylib`, queries its descriptor to list all exported functions and their signatures, calls `math_add`, `math_multiply`, and `math_sqrt`, probes for an optional `math_divide` (not exported), and checks for legacy convention symbols.

### Lifecycle demo — opt-in init/shutdown hooks

Uses `ILifecycleAware` to give plugins post-construction setup and pre-destruction teardown hooks. The host detects the mixin via `dynamic_cast` — same pattern as `IServiceAware`. Plugins that don't need lifecycle hooks simply don't inherit the mixin.

```bash
./build/bin/lifecycle_demo
```

The demo loads a `CountingWorker` plugin that uppercases strings and counts how many it has processed. The host calls `on_init()` after loading (resets the counter) and `on_shutdown()` before cleanup (prints a summary of items processed).

### Crash diagnostic — debug loading failures

A diagnostic tool that loads any `.dylib`/`.so`/`.dll` the same way a black-box app would, but with full error reporting at every stage. Tells you exactly where the load fails.

```bash
./build/bin/crash_diag /path/to/your/library.dylib
```

Includes three intentionally broken plugins that demonstrate common failure modes:

| Plugin | Failure | Diagnostic output |
|---|---|---|
| `libmissing_symbols.dylib` | No `extern "C"` — symbols are mangled | Step 2: symbol not found |
| `libcrash_in_constructor.dylib` | Constructor throws | Step 3: exception message |
| `libcrash_on_load.dylib` | Static initializer crashes inside `dlopen` | Step 1: process aborts |

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
#include "PluginFactory.hpp"

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
