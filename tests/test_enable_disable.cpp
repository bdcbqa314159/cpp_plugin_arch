#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
#include <string>

#include "IDependencyAware.hpp"
#include "IEventAware.hpp"
#include "ILifecycleAware.hpp"
#include "PluginManager.hpp"

using namespace plugin_arch;

// --- Test plugins ---

class TogglePlugin : public IPlugin,
                     public ILifecycleAware,
                     public IEventAware {
 public:
  const std::string& name() const override {
    static const std::string n = "TogglePlugin";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "toggle";
    return t;
  }

  void on_init() override { ++init_count; }
  void on_shutdown() override { ++shutdown_count; }

  void set_event_bus(EventBus& bus) override {
    (void)bus.subscribe("ping",
                        [this](const std::string&, const std::string&) {
                          ++ping_count;
                        });
  }

  int init_count = 0;
  int shutdown_count = 0;
  int ping_count = 0;
};

class SimplePlugin : public IPlugin, public ILifecycleAware {
 public:
  const std::string& name() const override {
    static const std::string n = "SimplePlugin";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "simple";
    return t;
  }
  void on_init() override { ++init_count; }
  void on_shutdown() override { ++shutdown_count; }

  int init_count = 0;
  int shutdown_count = 0;
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

TEST_CASE("Enable/Disable: is_enabled defaults to true", "[manager][toggle]") {
  PluginManager manager;
  auto plugin = std::make_shared<SimplePlugin>();
  manager.add_plugin(plugin, make_entry("SimplePlugin", "simple"));

  CHECK(manager.is_enabled("SimplePlugin"));
  CHECK_FALSE(manager.is_enabled("NonExistent"));

  manager.shutdown();
}

TEST_CASE("Enable/Disable: disable calls on_shutdown", "[manager][toggle]") {
  PluginManager manager;
  auto plugin = std::make_shared<SimplePlugin>();
  manager.add_plugin(plugin, make_entry("SimplePlugin", "simple"));

  CHECK(plugin->shutdown_count == 0);

  manager.disable("SimplePlugin");

  CHECK_FALSE(manager.is_enabled("SimplePlugin"));
  CHECK(plugin->shutdown_count == 1);
  CHECK(manager.is_loaded("SimplePlugin"));  // still loaded

  manager.shutdown();
}

TEST_CASE("Enable/Disable: disable is idempotent", "[manager][toggle]") {
  PluginManager manager;
  auto plugin = std::make_shared<SimplePlugin>();
  manager.add_plugin(plugin, make_entry("SimplePlugin", "simple"));

  manager.disable("SimplePlugin");
  manager.disable("SimplePlugin");  // second call is no-op

  CHECK(plugin->shutdown_count == 1);

  manager.shutdown();
}

TEST_CASE("Enable/Disable: enable re-wires and calls on_init",
          "[manager][toggle]") {
  PluginManager manager;
  auto plugin = std::make_shared<SimplePlugin>();
  manager.add_plugin(plugin, make_entry("SimplePlugin", "simple"));

  CHECK(plugin->init_count == 1);  // from add_plugin

  manager.disable("SimplePlugin");
  manager.enable("SimplePlugin");

  CHECK(manager.is_enabled("SimplePlugin"));
  CHECK(plugin->init_count == 2);  // re-initialized

  manager.shutdown();
}

TEST_CASE("Enable/Disable: enable is idempotent", "[manager][toggle]") {
  PluginManager manager;
  auto plugin = std::make_shared<SimplePlugin>();
  manager.add_plugin(plugin, make_entry("SimplePlugin", "simple"));

  manager.enable("SimplePlugin");  // already enabled, no-op

  CHECK(plugin->init_count == 1);

  manager.shutdown();
}

TEST_CASE("Enable/Disable: disable unsubscribes from EventBus",
          "[manager][toggle]") {
  PluginManager manager;
  auto plugin = std::make_shared<TogglePlugin>();
  manager.add_plugin(plugin, make_entry("TogglePlugin", "toggle"));

  // Verify subscription works
  manager.event_bus().publish("ping");
  CHECK(plugin->ping_count == 1);

  // Disable should unsubscribe
  manager.disable("TogglePlugin");

  manager.event_bus().publish("ping");
  CHECK(plugin->ping_count == 1);  // no increment

  manager.shutdown();
}

TEST_CASE("Enable/Disable: enable re-subscribes to EventBus",
          "[manager][toggle]") {
  PluginManager manager;
  auto plugin = std::make_shared<TogglePlugin>();
  manager.add_plugin(plugin, make_entry("TogglePlugin", "toggle"));

  manager.event_bus().publish("ping");
  CHECK(plugin->ping_count == 1);

  manager.disable("TogglePlugin");
  manager.enable("TogglePlugin");

  manager.event_bus().publish("ping");
  CHECK(plugin->ping_count == 2);

  manager.shutdown();
}

TEST_CASE("Enable/Disable: disable nonexistent throws", "[manager][toggle]") {
  PluginManager manager;
  CHECK_THROWS_AS(manager.disable("NonExistent"), std::runtime_error);
}

TEST_CASE("Enable/Disable: enable nonexistent throws", "[manager][toggle]") {
  PluginManager manager;
  CHECK_THROWS_AS(manager.enable("NonExistent"), std::runtime_error);
}

TEST_CASE("Enable/Disable: get_service skips disabled plugins",
          "[manager][toggle]") {
  PluginManager manager;
  auto plugin = std::make_shared<SimplePlugin>();
  manager.add_plugin(plugin, make_entry("SimplePlugin", "simple"));

  CHECK(manager.get_service<IPlugin>("simple") != nullptr);

  manager.disable("SimplePlugin");
  CHECK(manager.get_service<IPlugin>("simple") == nullptr);

  manager.enable("SimplePlugin");
  CHECK(manager.get_service<IPlugin>("simple") != nullptr);

  manager.shutdown();
}

// --- D7: disable cascades to dependents ---

class DepPlugin : public IPlugin,
                  public ILifecycleAware,
                  public IDependencyAware {
 public:
  DepPlugin(std::string name, std::string type, std::vector<std::string> deps)
      : name_(std::move(name)), type_(std::move(type)),
        deps_(std::move(deps)) {}

  const std::string& name() const override { return name_; }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override { return type_; }
  std::vector<std::string> dependencies() const override { return deps_; }
  void on_init() override { ++init_count; }
  void on_shutdown() override { ++shutdown_count; }

  int init_count = 0;
  int shutdown_count = 0;

 private:
  std::string name_;
  std::string type_;
  std::vector<std::string> deps_;
};

TEST_CASE("Enable/Disable: disable cascades to dependents (D7)",
          "[manager][toggle]") {
  PluginManager manager;
  auto base = std::make_shared<SimplePlugin>();
  auto dep = std::make_shared<DepPlugin>("DepPlugin", "dep", std::vector<std::string>{"simple"});

  manager.add_plugin(base, make_entry("SimplePlugin", "simple"));
  manager.add_plugin(dep, make_entry("DepPlugin", "dep"));

  CHECK(manager.is_enabled("SimplePlugin"));
  CHECK(manager.is_enabled("DepPlugin"));

  // Disabling base should cascade to dep
  manager.disable("SimplePlugin");

  CHECK_FALSE(manager.is_enabled("SimplePlugin"));
  CHECK_FALSE(manager.is_enabled("DepPlugin"));
  CHECK(base->shutdown_count == 1);
  CHECK(dep->shutdown_count == 1);

  // Both still loaded
  CHECK(manager.is_loaded("SimplePlugin"));
  CHECK(manager.is_loaded("DepPlugin"));

  manager.shutdown();
}

TEST_CASE("Enable/Disable: disable leaf does not cascade (D7)",
          "[manager][toggle]") {
  PluginManager manager;
  auto base = std::make_shared<SimplePlugin>();
  auto dep = std::make_shared<DepPlugin>("DepPlugin", "dep", std::vector<std::string>{"simple"});

  manager.add_plugin(base, make_entry("SimplePlugin", "simple"));
  manager.add_plugin(dep, make_entry("DepPlugin", "dep"));

  // Disabling the leaf should not affect base
  manager.disable("DepPlugin");

  CHECK(manager.is_enabled("SimplePlugin"));
  CHECK_FALSE(manager.is_enabled("DepPlugin"));

  manager.shutdown();
}

// --- D8: locator excludes disabled plugins ---

TEST_CASE("Enable/Disable: locator().get skips disabled plugins (D8)",
          "[manager][toggle]") {
  PluginManager manager;
  auto plugin = std::make_shared<SimplePlugin>();
  manager.add_plugin(plugin, make_entry("SimplePlugin", "simple"));

  // Direct locator access should find it when enabled
  CHECK(manager.locator().get<IPlugin>("simple") != nullptr);

  manager.disable("SimplePlugin");

  // Direct locator access should NOT find it when disabled
  CHECK(manager.locator().get<IPlugin>("simple") == nullptr);

  manager.enable("SimplePlugin");

  // Back after re-enable
  CHECK(manager.locator().get<IPlugin>("simple") != nullptr);

  manager.shutdown();
}
