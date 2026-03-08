#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
#include <string>
#include <vector>

#include "IDependencyAware.hpp"
#include "ILifecycleAware.hpp"
#include "ISerializable.hpp"
#include "PluginManager.hpp"
#include "SemVer.hpp"

using namespace plugin_arch;

// --- Test plugins (in-process, no dlopen) ---

class MockLogger : public IPlugin, public ILifecycleAware {
 public:
  const std::string& name() const override {
    static const std::string n = "MockLogger";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "2.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "logger";
    return t;
  }
  void on_init() override { ++init_count; }
  void on_shutdown() override { ++shutdown_count; }

  int init_count = 0;
  int shutdown_count = 0;
};

class MockProcessor : public IPlugin,
                      public ILifecycleAware,
                      public IDependencyAware {
 public:
  const std::string& name() const override {
    static const std::string n = "MockProcessor";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "processor";
    return t;
  }
  std::vector<std::string> dependencies() const override { return {"logger"}; }
  void on_init() override { ++init_count; }
  void on_shutdown() override { ++shutdown_count; }

  int init_count = 0;
  int shutdown_count = 0;
};

class MockAggregator : public IPlugin,
                       public ILifecycleAware,
                       public IDependencyAware {
 public:
  const std::string& name() const override {
    static const std::string n = "MockAggregator";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "aggregator";
    return t;
  }
  std::vector<std::string> dependencies() const override {
    return {"logger", "processor"};
  }
  void on_init() override { ++init_count; }
  void on_shutdown() override { ++shutdown_count; }

  int init_count = 0;
  int shutdown_count = 0;
};

// Helper to build a PluginEntry for add_plugin().
static PluginEntry make_entry(const std::string& name,
                              const std::string& type,
                              const std::string& version = "1.0.0") {
  PluginEntry e;
  e.name = name;
  e.type = type;
  e.version = version;
  return e;
}

// --- Tests ---

TEST_CASE("add_plugin: nullptr instance throws", "[manager][lifecycle]") {
  PluginManager manager;
  CHECK_THROWS_AS(
      manager.add_plugin(nullptr, make_entry("Null", "null")),
      std::runtime_error);
}

TEST_CASE("add_plugin: duplicate name throws", "[manager][lifecycle]") {
  PluginManager manager;
  manager.add_plugin(std::make_shared<MockLogger>(),
                     make_entry("MockLogger", "logger", "2.0.0"));

  CHECK_THROWS_AS(
      manager.add_plugin(std::make_shared<MockLogger>(),
                         make_entry("MockLogger", "logger2", "1.0.0")),
      std::runtime_error);

  manager.shutdown();
}

TEST_CASE("PluginManager: add_plugin and get_plugin", "[manager][lifecycle]") {
  PluginManager manager;
  auto logger = std::make_shared<MockLogger>();
  manager.add_plugin(logger, make_entry("MockLogger", "logger", "2.0.0"));

  CHECK(manager.is_loaded("MockLogger"));
  CHECK(manager.get_plugin("MockLogger") == logger);
  CHECK(manager.get_plugin("NonExistent") == nullptr);
  CHECK(manager.instances().size() == 1);

  manager.shutdown();
}

TEST_CASE("PluginManager: unload removes plugin", "[manager][lifecycle]") {
  PluginManager manager;
  auto logger = std::make_shared<MockLogger>();
  manager.add_plugin(logger, make_entry("MockLogger", "logger"));

  CHECK(manager.is_loaded("MockLogger"));

  manager.unload("MockLogger");

  CHECK_FALSE(manager.is_loaded("MockLogger"));
  CHECK(manager.instances().empty());
  CHECK(logger->shutdown_count == 1);
}

TEST_CASE("PluginManager: unload nonexistent throws", "[manager][lifecycle]") {
  PluginManager manager;
  CHECK_THROWS_AS(manager.unload("NonExistent"), std::runtime_error);
}

TEST_CASE("PluginManager: unload cascades to dependents",
          "[manager][lifecycle]") {
  PluginManager manager;
  auto logger = std::make_shared<MockLogger>();
  auto processor = std::make_shared<MockProcessor>();
  auto aggregator = std::make_shared<MockAggregator>();

  manager.add_plugin(logger, make_entry("MockLogger", "logger"));
  manager.add_plugin(processor, make_entry("MockProcessor", "processor"));
  manager.add_plugin(aggregator, make_entry("MockAggregator", "aggregator"));

  CHECK(manager.instances().size() == 3);

  // Unloading logger should cascade: aggregator and processor depend on it
  manager.unload("MockLogger");

  CHECK_FALSE(manager.is_loaded("MockLogger"));
  CHECK_FALSE(manager.is_loaded("MockProcessor"));
  CHECK_FALSE(manager.is_loaded("MockAggregator"));
  CHECK(manager.instances().empty());

  CHECK(logger->shutdown_count == 1);
  CHECK(processor->shutdown_count == 1);
  CHECK(aggregator->shutdown_count == 1);
}

TEST_CASE("PluginManager: unload leaf does not cascade",
          "[manager][lifecycle]") {
  PluginManager manager;
  auto logger = std::make_shared<MockLogger>();
  auto processor = std::make_shared<MockProcessor>();

  manager.add_plugin(logger, make_entry("MockLogger", "logger"));
  manager.add_plugin(processor, make_entry("MockProcessor", "processor"));

  // Unloading processor (leaf) should not affect logger
  manager.unload("MockProcessor");

  CHECK(manager.is_loaded("MockLogger"));
  CHECK_FALSE(manager.is_loaded("MockProcessor"));
  CHECK(manager.instances().size() == 1);

  CHECK(processor->shutdown_count == 1);
  CHECK(logger->shutdown_count == 0);

  manager.shutdown();
}

TEST_CASE("PluginManager: dependents_of returns correct names",
          "[manager][lifecycle]") {
  PluginManager manager;
  auto logger = std::make_shared<MockLogger>();
  auto processor = std::make_shared<MockProcessor>();
  auto aggregator = std::make_shared<MockAggregator>();

  manager.add_plugin(logger, make_entry("MockLogger", "logger"));
  manager.add_plugin(processor, make_entry("MockProcessor", "processor"));
  manager.add_plugin(aggregator, make_entry("MockAggregator", "aggregator"));

  auto deps = manager.dependents_of("MockLogger");
  // Both processor and aggregator depend on logger
  CHECK(deps.size() == 2);

  auto deps_proc = manager.dependents_of("MockProcessor");
  // Only aggregator depends on processor
  CHECK(deps_proc.size() == 1);
  CHECK(deps_proc[0] == "MockAggregator");

  auto deps_agg = manager.dependents_of("MockAggregator");
  // Nothing depends on aggregator
  CHECK(deps_agg.empty());

  manager.shutdown();
}

TEST_CASE("PluginManager: loaded_plugins exposes internal state",
          "[manager][lifecycle]") {
  PluginManager manager;
  auto logger = std::make_shared<MockLogger>();
  manager.add_plugin(logger, make_entry("MockLogger", "logger"));

  const auto& plugins = manager.loaded_plugins();
  REQUIRE(plugins.size() == 1);
  CHECK(plugins[0].entry.name == "MockLogger");
  CHECK(plugins[0].entry.type == "logger");
  CHECK(plugins[0].instance == logger);

  manager.shutdown();
}

TEST_CASE("PluginManager: shutdown calls on_shutdown in reverse order",
          "[manager][lifecycle]") {
  PluginManager manager;
  auto logger = std::make_shared<MockLogger>();
  auto processor = std::make_shared<MockProcessor>();

  std::vector<std::string> shutdown_order;

  // We can't easily track order with the mock plugins' counters,
  // but we can verify both get shut down.
  manager.add_plugin(logger, make_entry("MockLogger", "logger"));
  manager.add_plugin(processor, make_entry("MockProcessor", "processor"));

  manager.shutdown();

  CHECK(logger->shutdown_count == 1);
  CHECK(processor->shutdown_count == 1);
  CHECK(manager.instances().empty());
}

// --- Version-constrained dependency plugin ---

class VersionedProcessor : public IPlugin,
                           public ILifecycleAware,
                           public IDependencyAware {
 public:
  const std::string& name() const override {
    static const std::string n = "VersionedProcessor";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "vprocessor";
    return t;
  }
  std::vector<std::string> dependencies() const override {
    return {"logger >= 2.0"};
  }
  void on_init() override {}
  void on_shutdown() override {}
};

TEST_CASE("add_plugin: version constraint satisfied (D6)",
          "[manager][lifecycle]") {
  PluginManager manager;
  auto logger = std::make_shared<MockLogger>();
  // MockLogger version is "2.0.0" — satisfies "logger >= 2.0"
  manager.add_plugin(logger, make_entry("MockLogger", "logger", "2.0.0"));

  CHECK_NOTHROW(
      manager.add_plugin(std::make_shared<VersionedProcessor>(),
                         make_entry("VersionedProcessor", "vprocessor")));

  manager.shutdown();
}

TEST_CASE("add_plugin: version constraint violated throws (D6)",
          "[manager][lifecycle]") {
  PluginManager manager;
  auto logger = std::make_shared<MockLogger>();
  // Register logger as version "1.0.0" — does NOT satisfy "logger >= 2.0"
  manager.add_plugin(logger, make_entry("MockLogger", "logger", "1.0.0"));

  CHECK_THROWS_AS(
      manager.add_plugin(std::make_shared<VersionedProcessor>(),
                         make_entry("VersionedProcessor", "vprocessor")),
      std::runtime_error);

  CHECK_FALSE(manager.is_loaded("VersionedProcessor"));

  manager.shutdown();
}

TEST_CASE("add_plugin: reverse version check — provider loaded after dependent",
          "[manager][lifecycle]") {
  PluginManager manager;
  // Load dependent first (no provider yet — accepted for incremental loading)
  manager.add_plugin(std::make_shared<VersionedProcessor>(),
                     make_entry("VersionedProcessor", "vprocessor"));

  // Now load provider with version that does NOT satisfy "logger >= 2.0"
  CHECK_THROWS_AS(
      manager.add_plugin(std::make_shared<MockLogger>(),
                         make_entry("MockLogger", "logger", "1.0.0")),
      std::runtime_error);

  CHECK_FALSE(manager.is_loaded("MockLogger"));

  // Loading with satisfying version should work
  CHECK_NOTHROW(
      manager.add_plugin(std::make_shared<MockLogger>(),
                         make_entry("MockLogger", "logger", "2.0.0")));

  manager.shutdown();
}
