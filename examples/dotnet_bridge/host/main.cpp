// Host that loads the .NET bridge plugin via DynamicLibrary — exactly like
// dynamic_plugin_demo, with additional demos for batch and string tiers
// plus a performance benchmark section.

#include "DynamicLibrary.hpp"
#include "PluginDescriptor.hpp"
#include "platform/shared_lib.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
  namespace fs = std::filesystem;

  auto lib_name =
      "dotnet_bridge" +
      std::string(plugin_arch::platform::shared_lib_extension());
  auto plugin_dir = fs::path(argv[0]).parent_path();
  if (argc > 1) {
    plugin_dir = argv[1];
  }
  auto path = (plugin_dir / lib_name).string();

  std::cout << "Loading: " << lib_name << "\n";

  try {
    plugin_arch::DynamicLibrary lib(path);

    // --- Step 1: Discover the API via plugin_describe() ---
    using DescribeFunc = const plugin_arch::PluginDescriptor* (*)();
    auto describe = lib.resolve<DescribeFunc>("plugin_describe");
    auto* desc = describe();

    std::cout << "\nPlugin: " << desc->name << " v" << desc->version << "\n";
    std::cout << "Exported functions (" << desc->function_count << "):\n";
    for (int i = 0; i < desc->function_count; ++i) {
      auto name = std::string(desc->functions[i].name);
      auto pad_len = (name.size() < 20) ? 20 - name.size() : 1;
      auto pad = std::string(pad_len, ' ');
      std::cout << "  " << name << pad << ": "
                << desc->functions[i].signature << "\n";
    }

    // --- Step 2: Scalar calls (same as dynamic_plugin_demo) ---
    using BinaryFunc = double (*)(double, double);
    using UnaryFunc = double (*)(double);

    auto add = lib.resolve<BinaryFunc>("math_add");
    auto multiply = lib.resolve<BinaryFunc>("math_multiply");
    auto sqrt_fn = lib.resolve<UnaryFunc>("math_sqrt");

    std::cout << "\nScalar calls:\n";
    std::cout << "  math_add(10, 3)      = " << add(10, 3) << "\n";
    std::cout << "  math_multiply(10, 3) = " << multiply(10, 3) << "\n";
    std::cout << "  math_sqrt(144)       = " << sqrt_fn(144) << "\n";

    // --- Step 3: Batch call demo ---
    using BatchFunc = void (*)(double*, int, double);
    auto batch_multiply = lib.resolve<BatchFunc>("math_batch_multiply");

    std::vector<double> values = {1.0, 2.0, 3.0, 4.0, 5.0,
                                  6.0, 7.0, 8.0, 9.0, 10.0};
    const double factor = 2.5;

    std::cout << "\nBatch multiply (x " << factor << "):\n";
    std::cout << "  Before:";
    for (auto v : values) std::cout << " " << v;
    std::cout << "\n";

    batch_multiply(values.data(), static_cast<int>(values.size()), factor);

    std::cout << "  After: ";
    for (auto v : values) std::cout << " " << v;
    std::cout << "\n";

    // --- Step 4: String marshal demo ---
    using FormatFunc = char* (*)(const char*, double);
    using FreeFunc = void (*)(char*);

    auto format_result = lib.resolve<FormatFunc>("math_format_result");
    auto free_string = lib.resolve<FreeFunc>("math_free_string");

    char* formatted = format_result("answer", 42.0);
    std::cout << "\nString marshal:\n";
    std::cout << "  math_format_result(\"answer\", 42.0) = \""
              << formatted << "\"\n";
    free_string(formatted);

    // --- Step 5: Probe for optional functions ---
    std::cout << "\nProbing for optional functions:\n";
    std::cout << "  math_divide:     "
              << (lib.has("math_divide") ? "available" : "not available")
              << "\n";

    std::cout << "\nConvention check:\n";
    std::cout << "  has 'allocator':       "
              << (lib.has("allocator") ? "yes" : "no") << "\n";
    std::cout << "  has 'deallocator':     "
              << (lib.has("deallocator") ? "yes" : "no") << "\n";
    std::cout << "  has 'plugin_describe': "
              << (lib.has("plugin_describe") ? "yes" : "no") << "\n";

    // --- Step 6: Performance benchmark ---
    std::cout << "\n--- Performance benchmark ---\n";
    using Clock = std::chrono::high_resolution_clock;

    // Scalar: 100K iterations
    {
      constexpr int N = 100'000;
      auto start = Clock::now();
      volatile double sink = 0;
      for (int i = 0; i < N; ++i) {
        sink = add(static_cast<double>(i), 1.0);
      }
      auto elapsed = Clock::now() - start;
      auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed);
      std::cout << "  Scalar (math_add, " << N << " calls): "
                << ns.count() / N << " ns/call\n";
    }

    // Batch: 1K iterations x 1000 elements
    {
      constexpr int ITERS = 1'000;
      constexpr int SIZE = 1'000;
      std::vector<double> buf(SIZE, 1.0);
      auto start = Clock::now();
      for (int i = 0; i < ITERS; ++i) {
        batch_multiply(buf.data(), SIZE, 1.0);  // factor=1.0 to avoid overflow
      }
      auto elapsed = Clock::now() - start;
      auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed);
      std::cout << "  Batch  (" << ITERS << "x" << SIZE << " elements): "
                << ns.count() / ITERS << " ns/call\n";
    }

    // String: 10K iterations
    {
      constexpr int N = 10'000;
      auto start = Clock::now();
      for (int i = 0; i < N; ++i) {
        char* s = format_result("x", static_cast<double>(i));
        free_string(s);
      }
      auto elapsed = Clock::now() - start;
      auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed);
      std::cout << "  String (format+free, " << N << " calls): "
                << ns.count() / N << " ns/call\n";
    }

  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
