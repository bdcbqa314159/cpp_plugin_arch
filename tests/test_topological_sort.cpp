#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <stdexcept>
#include <unordered_map>

#include "PluginManager.hpp"

using namespace plugin_arch;

// Helper: build DiscoveredPlugin + type_to_index from a simple list of {name/type, deps}.
static std::pair<std::vector<DiscoveredPlugin>,
                 std::unordered_map<std::string, std::size_t>>
make_graph(std::vector<std::pair<std::string, std::vector<std::string>>> nodes) {
  std::vector<DiscoveredPlugin> infos;
  std::unordered_map<std::string, std::size_t> type_to_index;

  for (auto& [type, deps] : nodes) {
    DiscoveredPlugin info;
    info.entry.name = type;
    info.entry.type = type;
    info.deps = std::move(deps);
    type_to_index[type] = infos.size();
    infos.push_back(std::move(info));
  }
  return {std::move(infos), std::move(type_to_index)};
}

TEST_CASE("Topological sort: no dependencies", "[topo]") {
  auto [infos, idx] = make_graph({{"A", {}}, {"B", {}}, {"C", {}}});
  auto levels = topological_sort_levels(infos, idx);

  // All independent — single level
  REQUIRE(levels.size() == 1);
  CHECK(levels[0].size() == 3);
}

TEST_CASE("Topological sort: linear chain", "[topo]") {
  // C → B → A
  auto [infos, idx] = make_graph({
      {"A", {}},
      {"B", {"A"}},
      {"C", {"B"}},
  });
  auto levels = topological_sort_levels(infos, idx);

  REQUIRE(levels.size() == 3);
  CHECK(infos[levels[0][0]].entry.type == "A");
  CHECK(infos[levels[1][0]].entry.type == "B");
  CHECK(infos[levels[2][0]].entry.type == "C");
}

TEST_CASE("Topological sort: diamond dependency", "[topo]") {
  //   A
  //  / \
  // B   C
  //  \ /
  //   D
  auto [infos, idx] = make_graph({
      {"A", {}},
      {"B", {"A"}},
      {"C", {"A"}},
      {"D", {"B", "C"}},
  });
  auto levels = topological_sort_levels(infos, idx);

  REQUIRE(levels.size() == 3);
  CHECK(levels[0].size() == 1);
  CHECK(infos[levels[0][0]].entry.type == "A");
  CHECK(levels[1].size() == 2);
  CHECK(levels[2].size() == 1);
  CHECK(infos[levels[2][0]].entry.type == "D");
}

TEST_CASE("Topological sort: missing dependency throws", "[topo]") {
  auto [infos, idx] = make_graph({{"A", {"nonexistent"}}});
  CHECK_THROWS_AS(topological_sort_levels(infos, idx), std::runtime_error);
}

TEST_CASE("Topological sort: cycle detection", "[topo]") {
  auto [infos, idx] = make_graph({
      {"A", {"B"}},
      {"B", {"A"}},
  });
  CHECK_THROWS_AS(topological_sort_levels(infos, idx), std::runtime_error);
}

TEST_CASE("Topological sort: empty input", "[topo]") {
  std::vector<DiscoveredPlugin> infos;
  std::unordered_map<std::string, std::size_t> idx;
  auto levels = topological_sort_levels(infos, idx);
  CHECK(levels.empty());
}

TEST_CASE("Topological sort: self-dependency is a cycle", "[topo]") {
  auto [infos, idx] = make_graph({{"A", {"A"}}});
  CHECK_THROWS_AS(topological_sort_levels(infos, idx), std::runtime_error);
}

TEST_CASE("PluginManager load_all with empty directory is a no-op", "[manager]") {
  PluginManager manager;
  auto tmp = std::filesystem::temp_directory_path();
  CHECK_NOTHROW(manager.load_all(tmp));
  CHECK(manager.instances().empty());
}

TEST_CASE("PluginManager load_all with nonexistent directory throws", "[manager]") {
  PluginManager manager;
  CHECK_THROWS_AS(manager.load_all("/nonexistent/path"), LoadError);
}
