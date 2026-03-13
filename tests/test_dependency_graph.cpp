#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <string>
#include <vector>

#include "IDependencyAware.hpp"
#include "ILifecycleAware.hpp"
#include "PluginManager.hpp"

using namespace plugin_arch;
using Catch::Matchers::ContainsSubstring;

// --- Test plugins ---

class BasePlugin : public IPlugin, public ILifecycleAware {
 public:
  const std::string& name() const override {
    static const std::string n = "Base";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "base";
    return t;
  }
  void on_init() override {}
  void on_shutdown() override {}
};

class MiddlePlugin : public IPlugin,
                     public ILifecycleAware,
                     public IDependencyAware {
 public:
  const std::string& name() const override {
    static const std::string n = "Middle";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "middle";
    return t;
  }
  std::vector<std::string> dependencies() const override { return {"base"}; }
  void on_init() override {}
  void on_shutdown() override {}
};

class TopPlugin : public IPlugin,
                  public ILifecycleAware,
                  public IDependencyAware {
 public:
  const std::string& name() const override {
    static const std::string n = "Top";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "top";
    return t;
  }
  std::vector<std::string> dependencies() const override {
    return {"base", "middle"};
  }
  void on_init() override {}
  void on_shutdown() override {}
};

static PluginEntry make_entry(const std::string& name,
                              const std::string& type) {
  PluginEntry e;
  e.name = name;
  e.type = type;
  e.version = "1.0.0";
  return e;
}

// --- Tests ---

TEST_CASE("Dependency graph: empty manager", "[depgraph]") {
  PluginManager manager;
  CHECK(manager.dependency_graph().empty());
}

TEST_CASE("Dependency graph: single plugin no deps", "[depgraph]") {
  PluginManager manager;
  manager.add_plugin(std::make_shared<BasePlugin>(),
                     make_entry("Base", "base"));

  auto graph = manager.dependency_graph();
  REQUIRE(graph.size() == 1);
  CHECK(graph[0] == "Base (base) [no deps]");

  manager.shutdown();
}

TEST_CASE("Dependency graph: chain with deps", "[depgraph]") {
  PluginManager manager;
  manager.add_plugin(std::make_shared<BasePlugin>(),
                     make_entry("Base", "base"));
  manager.add_plugin(std::make_shared<MiddlePlugin>(),
                     make_entry("Middle", "middle"));
  manager.add_plugin(std::make_shared<TopPlugin>(),
                     make_entry("Top", "top"));

  auto graph = manager.dependency_graph();
  REQUIRE(graph.size() == 3);
  CHECK(graph[0] == "Base (base) [no deps]");
  CHECK(graph[1] == "Middle (middle) -> base");
  CHECK(graph[2] == "Top (top) -> base, middle");

  manager.shutdown();
}

// --- C3: Cycle chain visualization ---

TEST_CASE("Cycle detection: error shows cycle path", "[depgraph][cycle]") {
  // Create plugins that form a cycle: A -> B -> C -> A
  class CycleA : public IPlugin, public IDependencyAware {
   public:
    const std::string& name() const override {
      static const std::string n = "CycleA";
      return n;
    }
    const std::string& version() const override {
      static const std::string v = "1.0.0";
      return v;
    }
    const std::string& type() const override {
      static const std::string t = "cycleA";
      return t;
    }
    std::vector<std::string> dependencies() const override {
      return {"cycleC"};
    }
  };

  class CycleB : public IPlugin, public IDependencyAware {
   public:
    const std::string& name() const override {
      static const std::string n = "CycleB";
      return n;
    }
    const std::string& version() const override {
      static const std::string v = "1.0.0";
      return v;
    }
    const std::string& type() const override {
      static const std::string t = "cycleB";
      return t;
    }
    std::vector<std::string> dependencies() const override {
      return {"cycleA"};
    }
  };

  class CycleC : public IPlugin, public IDependencyAware {
   public:
    const std::string& name() const override {
      static const std::string n = "CycleC";
      return n;
    }
    const std::string& version() const override {
      static const std::string v = "1.0.0";
      return v;
    }
    const std::string& type() const override {
      static const std::string t = "cycleC";
      return t;
    }
    std::vector<std::string> dependencies() const override {
      return {"cycleB"};
    }
  };

  // Test via topological_sort_levels directly
  std::vector<DiscoveredPlugin> infos(3);
  infos[0].entry.name = "CycleA";
  infos[0].entry.type = "cycleA";
  infos[0].deps = {"cycleC"};
  infos[1].entry.name = "CycleB";
  infos[1].entry.type = "cycleB";
  infos[1].deps = {"cycleA"};
  infos[2].entry.name = "CycleC";
  infos[2].entry.type = "cycleC";
  infos[2].deps = {"cycleB"};

  std::unordered_map<std::string, std::size_t> type_to_index = {
      {"cycleA", 0}, {"cycleB", 1}, {"cycleC", 2}};

  try {
    topological_sort_levels(infos, type_to_index);
    FAIL("Should have thrown");
  } catch (const std::runtime_error& e) {
    std::string msg = e.what();
    // Should contain "->" showing the cycle path
    CHECK_THAT(msg, ContainsSubstring("->"));
    CHECK_THAT(msg, ContainsSubstring("Circular dependency detected"));
  }
}

TEST_CASE("Cycle detection: two-node cycle shows path", "[depgraph][cycle]") {
  std::vector<DiscoveredPlugin> infos(2);
  infos[0].entry.name = "Alpha";
  infos[0].entry.type = "alpha";
  infos[0].deps = {"beta"};
  infos[1].entry.name = "Beta";
  infos[1].entry.type = "beta";
  infos[1].deps = {"alpha"};

  std::unordered_map<std::string, std::size_t> type_to_index = {
      {"alpha", 0}, {"beta", 1}};

  try {
    topological_sort_levels(infos, type_to_index);
    FAIL("Should have thrown");
  } catch (const std::runtime_error& e) {
    std::string msg = e.what();
    CHECK_THAT(msg, ContainsSubstring("->"));
    // Should mention both plugins
    CHECK_THAT(msg, ContainsSubstring("Alpha"));
    CHECK_THAT(msg, ContainsSubstring("Beta"));
  }
}
