#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
#include <string>

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
