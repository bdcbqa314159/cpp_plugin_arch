# cpp_plugin_arch

A C++20 plugin architecture library: runtime polymorphism across shared library boundaries. The host loads plugins at runtime knowing only the interface — no recompilation needed when plugins are added or swapped.

## Project structure

```
cpp_plugin_arch/
├── plugin_arch/                        # Reusable header-only library
│   ├── plugin_arch                     # Umbrella include
│   ├── IPlugin.hpp                     # Base interface (name, version, type)
│   ├── PluginLoader.hpp                # RAII cross-platform loader (dlopen/LoadLibrary)
│   ├── PluginFactory.hpp               # REGISTER_PLUGIN() macro
│   └── platform/
│       ├── exported.hpp                # Symbol visibility macros
│       └── abi.hpp                     # extern "C" wrapper macro
│
├── examples/                           # Full working example
│   ├── interfaces/
│   │   └── ICalculator.hpp             # Domain interface (extends IPlugin)
│   ├── plugins/
│   │   ├── basic_calc/                 # Plugin A: arithmetic only
│   │   └── scientific_calc/            # Plugin B: adds pow, sqrt
│   └── host/
│       └── main.cpp                    # Discovers and loads plugins at runtime
```

## How it works

1. **Define an interface** — a pure virtual class extending `plugin_arch::IPlugin`
2. **Implement plugins** — concrete classes that implement the interface, each compiled to a `.so`/`.dylib`/`.dll`
3. **Register plugins** — one line: `REGISTER_PLUGIN(MyClass)` generates the `extern "C"` factory
4. **Load from the host** — `PluginLoader<ICalculator>` opens the library, resolves symbols, returns a `shared_ptr`

The host never sees plugin source code. It only needs the interface header and the compiled binary.

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

std::cout << plugin->name() << "\n";     // "MyPlugin"
std::cout << plugin->add(10, 3) << "\n";  // 13
```

## Build

```bash
cmake -B build -G Ninja
cmake --build build
```

## Run

```bash
./build/bin/host
```

Output:

```
Found 2 plugin(s) in "./build/bin"

[BasicCalc v1.0.0]  (type: calculator)
  loaded from: "libbasic_calc.dylib"
  add(10, 3)      = 13
  subtract(10, 3) = 7
  multiply(10, 3) = 30
  divide(10, 3)   = 3.33333
  power(2, 8)     = [not supported]
  sqrt(144)       = [not supported]

[ScientificCalc v1.0.0]  (type: calculator)
  loaded from: "libscientific_calc.dylib"
  add(10, 3)      = 13
  subtract(10, 3) = 7
  multiply(10, 3) = 30
  divide(10, 3)   = 3.33333
  power(2, 8)     = 256
  sqrt(144)       = 12
```

## Docker (Linux on macOS)

```bash
docker build -t cpp_plugin_arch .
docker run --rm -v $(pwd):/workspace cpp_plugin_arch
```
