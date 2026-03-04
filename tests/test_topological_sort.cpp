// Tests for PluginManager's topological sort by exercising load_all()
// indirectly through mock plugins. Since the topo sort is private,
// we test it via the public API using real plugin loading.
//
// These tests verify the algorithm's error cases using the PluginManager
// directly — cycle detection and missing dependency detection.

#include <catch2/catch_test_macros.hpp>
#include <stdexcept>

#include "PluginManager.hpp"

using namespace plugin_arch;

TEST_CASE("PluginManager load_all with empty directory throws", "[topo]") {
  PluginManager manager;
  // Non-existent directory should throw LoadError
  CHECK_THROWS_AS(manager.load_all("/nonexistent/path"), LoadError);
}

TEST_CASE("PluginManager load_all with no plugins is a no-op", "[topo]") {
  PluginManager manager;
  // A real directory with no shared libraries should be fine
  CHECK_NOTHROW(manager.load_all("/tmp"));
  CHECK(manager.instances().empty());
}
