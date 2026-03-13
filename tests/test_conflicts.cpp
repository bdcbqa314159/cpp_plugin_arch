#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

#include "IConflictAware.hpp"
#include "PluginManager.hpp"

using namespace plugin_arch;

// --- Test plugins ---

class PluginA : public IPlugin, public IConflictAware {
 public:
  const std::string& name() const override {
    static const std::string n = "PluginA";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "typeA";
    return t;
  }
  std::vector<std::string> conflicts() const override { return {"typeB"}; }
};

class PluginB : public IPlugin {
 public:
  const std::string& name() const override {
    static const std::string n = "PluginB";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "typeB";
    return t;
  }
};

class NoConflictPlugin : public IPlugin {
 public:
  const std::string& name() const override {
    static const std::string n = "NoConflict";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "typeC";
    return t;
  }
};

static PluginEntry make_entry(const std::string& name,
                              const std::string& type) {
  PluginEntry e;
  e.name = name;
  e.type = type;
  e.version = "1.0.0";
  return e;
}

// PluginB declares conflict with typeA (reverse direction test).
class PluginBConflictsA : public IPlugin, public IConflictAware {
 public:
  const std::string& name() const override {
    static const std::string n = "PluginBConflictsA";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "typeB";
    return t;
  }
  std::vector<std::string> conflicts() const override { return {"typeA"}; }
};

// --- Tests ---

TEST_CASE("IConflictAware: returns conflict types", "[conflicts]") {
  PluginA a;
  auto conflicts = a.conflicts();
  REQUIRE(conflicts.size() == 1);
  CHECK(conflicts[0] == "typeB");
}

TEST_CASE("IConflictAware: dynamic_cast detection", "[conflicts]") {
  auto a = std::make_shared<PluginA>();
  auto b = std::make_shared<PluginB>();

  CHECK(dynamic_cast<IConflictAware*>(a.get()) != nullptr);
  CHECK(dynamic_cast<IConflictAware*>(b.get()) == nullptr);
}

TEST_CASE("DiscoveredPlugin: conflicts field populated correctly", "[conflicts]") {
  DiscoveredPlugin info;
  info.entry = make_entry("PluginA", "typeA");
  info.conflicts = {"typeB", "typeC"};

  CHECK(info.conflicts.size() == 2);
  CHECK(info.conflicts[0] == "typeB");
  CHECK(info.conflicts[1] == "typeC");
}

TEST_CASE("Non-conflicting plugins coexist via add_plugin", "[conflicts]") {
  PluginManager manager;
  manager.add_plugin(std::make_shared<PluginA>(),
                     make_entry("PluginA", "typeA"));
  manager.add_plugin(std::make_shared<NoConflictPlugin>(),
                     make_entry("NoConflict", "typeC"));

  CHECK(manager.is_loaded("PluginA"));
  CHECK(manager.is_loaded("NoConflict"));

  manager.shutdown();
}

TEST_CASE("add_plugin: new plugin conflicts with existing (D5)",
          "[conflicts]") {
  PluginManager manager;
  // PluginA declares conflict with typeB
  manager.add_plugin(std::make_shared<PluginA>(),
                     make_entry("PluginA", "typeA"));

  // Adding PluginB (typeB) should throw because PluginA conflicts with typeB
  CHECK_THROWS_AS(
      manager.add_plugin(std::make_shared<PluginB>(),
                         make_entry("PluginB", "typeB")),
      std::runtime_error);

  CHECK(manager.is_loaded("PluginA"));
  CHECK_FALSE(manager.is_loaded("PluginB"));

  manager.shutdown();
}

TEST_CASE("add_plugin: existing plugin conflicts with new (D5 reverse)",
          "[conflicts]") {
  PluginManager manager;
  // Load a plain PluginB (no conflicts declared on its side)
  manager.add_plugin(std::make_shared<PluginB>(),
                     make_entry("PluginB", "typeB"));

  // Now load PluginBConflictsA which declares conflict with typeA... no issue
  // But if we load something that an existing plugin conflicts with:
  // PluginA declares conflict with typeB, and typeB is already loaded
  CHECK_THROWS_AS(
      manager.add_plugin(std::make_shared<PluginA>(),
                         make_entry("PluginA", "typeA")),
      std::runtime_error);

  manager.shutdown();
}

TEST_CASE("add_plugin: reverse conflict check — existing declares conflict (D5)",
          "[conflicts]") {
  PluginManager manager;
  // Load PluginBConflictsA which declares conflict with typeA
  manager.add_plugin(std::make_shared<PluginBConflictsA>(),
                     make_entry("PluginBConflictsA", "typeB"));

  // Now add a plain plugin with typeA — should be blocked by existing conflict
  CHECK_THROWS_AS(
      manager.add_plugin(std::make_shared<NoConflictPlugin>(),
                         [&] {
                           auto e = make_entry("PlainA", "typeA");
                           return e;
                         }()),
      std::runtime_error);

  manager.shutdown();
}
