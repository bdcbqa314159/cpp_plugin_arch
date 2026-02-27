# cpp_plugin_arch

A C++20 project exploring plugin architecture patterns: dynamic loading, stable ABIs, interface contracts, and cross-platform shared library design.

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
