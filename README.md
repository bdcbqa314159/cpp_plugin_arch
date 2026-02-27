# cpp_plugin_arch

A C++20 plugin architecture library: runtime polymorphism across shared library boundaries. The host loads plugins at runtime knowing only the interface — no recompilation needed when plugins are added or swapped.

## Project structure

```
cpp_plugin_arch/
├── plugin_arch/                            # Reusable header-only library
│   ├── plugin_arch                         # Umbrella include
│   ├── IPlugin.hpp                         # Base interface (name, version, type)
│   ├── PluginLoader.hpp                    # RAII cross-platform loader (dlopen/LoadLibrary)
│   ├── PluginFactory.hpp                   # REGISTER_PLUGIN() macro
│   └── platform/
│       ├── exported.hpp                    # Symbol visibility macros
│       └── abi.hpp                         # extern "C" wrapper macro
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
