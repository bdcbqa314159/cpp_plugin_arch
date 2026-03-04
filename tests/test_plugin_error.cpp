#include <catch2/catch_test_macros.hpp>

#include "PluginError.hpp"

using namespace plugin_arch;

TEST_CASE("LoadError carries path and platform error", "[errors]") {
  auto err = LoadError::make("/path/to/lib.so", "file not found");
  CHECK(err.library_path() == "/path/to/lib.so");
  CHECK(err.platform_error() == "file not found");
  CHECK(std::string(err.what()).find("lib.so") != std::string::npos);
  CHECK(std::string(err.what()).find("file not found") != std::string::npos);
}

TEST_CASE("SymbolError carries path, symbol, and platform error", "[errors]") {
  auto err = SymbolError::make("/path/to/lib.so", "allocator", "symbol not found");
  CHECK(err.library_path() == "/path/to/lib.so");
  CHECK(err.symbol_name() == "allocator");
  CHECK(err.platform_error() == "symbol not found");
  CHECK(std::string(err.what()).find("allocator") != std::string::npos);
}

TEST_CASE("AllocationError carries path", "[errors]") {
  auto err = AllocationError::make("/path/to/lib.so");
  CHECK(err.library_path() == "/path/to/lib.so");
  CHECK(std::string(err.what()).find("nullptr") != std::string::npos);
}

TEST_CASE("InterfaceError carries path and requested type", "[errors]") {
  auto err = InterfaceError::make("/path/to/lib.so", "ICalculator");
  CHECK(err.library_path() == "/path/to/lib.so");
  CHECK(err.requested_type() == "ICalculator");
  CHECK(std::string(err.what()).find("ICalculator") != std::string::npos);
}

TEST_CASE("PluginError hierarchy inherits std::runtime_error", "[errors]") {
  auto err = LoadError::make("/lib.so", "test");

  // Can catch as PluginError
  CHECK_NOTHROW([&]() {
    try { throw err; }
    catch (const PluginError& e) { CHECK(e.library_path() == "/lib.so"); }
  }());

  // Can catch as std::runtime_error
  CHECK_NOTHROW([&]() {
    try { throw err; }
    catch (const std::runtime_error&) {}
  }());

  // Can catch as std::exception
  CHECK_NOTHROW([&]() {
    try { throw err; }
    catch (const std::exception&) {}
  }());
}
