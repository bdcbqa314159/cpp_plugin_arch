#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

#include "ILifecycleAware.hpp"
#include "PluginManager.hpp"
#include "PluginObserver.hpp"

using namespace plugin_arch;

// --- Recording observer ---

struct Event {
  std::string action;
  std::string name;
  std::string type;
};

class RecordingObserver : public PluginObserver {
 public:
  void on_plugin_loaded(const std::string& name,
                        const std::string& type,
                        const PluginEntry&) override {
    events.push_back({"loaded", name, type});
  }
  void on_plugin_unloaded(const std::string& name,
                          const std::string& type,
                          const PluginEntry&) override {
    events.push_back({"unloaded", name, type});
  }
  void on_plugin_reloaded(const std::string& name,
                          const std::string& type,
                          const PluginEntry&) override {
    events.push_back({"reloaded", name, type});
  }
  void on_plugin_enabled(const std::string& name,
                         const std::string& type,
                         const PluginEntry&) override {
    events.push_back({"enabled", name, type});
  }
  void on_plugin_disabled(const std::string& name,
                          const std::string& type,
                          const PluginEntry&) override {
    events.push_back({"disabled", name, type});
  }

  std::vector<Event> events;
};

// --- Test plugin ---

class ObsPlugin : public IPlugin, public ILifecycleAware {
 public:
  const std::string& name() const override {
    static const std::string n = "ObsPlugin";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "obs";
    return t;
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

TEST_CASE("Observer: on_plugin_loaded fires on add_plugin", "[observer]") {
  PluginManager manager;
  RecordingObserver obs;
  manager.add_observer(&obs);

  manager.add_plugin(std::make_shared<ObsPlugin>(),
                     make_entry("ObsPlugin", "obs"));

  REQUIRE(obs.events.size() == 1);
  CHECK(obs.events[0].action == "loaded");
  CHECK(obs.events[0].name == "ObsPlugin");
  CHECK(obs.events[0].type == "obs");

  manager.shutdown();
}

TEST_CASE("Observer: on_plugin_unloaded fires on unload", "[observer]") {
  PluginManager manager;
  RecordingObserver obs;
  manager.add_observer(&obs);

  manager.add_plugin(std::make_shared<ObsPlugin>(),
                     make_entry("ObsPlugin", "obs"));
  obs.events.clear();

  manager.unload("ObsPlugin");

  REQUIRE(obs.events.size() == 1);
  CHECK(obs.events[0].action == "unloaded");
  CHECK(obs.events[0].name == "ObsPlugin");
}

TEST_CASE("Observer: on_plugin_disabled/enabled fires", "[observer]") {
  PluginManager manager;
  RecordingObserver obs;
  manager.add_observer(&obs);

  manager.add_plugin(std::make_shared<ObsPlugin>(),
                     make_entry("ObsPlugin", "obs"));
  obs.events.clear();

  manager.disable("ObsPlugin");
  REQUIRE(obs.events.size() == 1);
  CHECK(obs.events[0].action == "disabled");

  manager.enable("ObsPlugin");
  REQUIRE(obs.events.size() == 2);
  CHECK(obs.events[1].action == "enabled");

  manager.shutdown();
}

TEST_CASE("Observer: no observer = no crash", "[observer]") {
  PluginManager manager;
  // No observers registered
  CHECK_NOTHROW(manager.add_plugin(std::make_shared<ObsPlugin>(),
                                   make_entry("ObsPlugin", "obs")));
  CHECK_NOTHROW(manager.disable("ObsPlugin"));
  CHECK_NOTHROW(manager.enable("ObsPlugin"));
  CHECK_NOTHROW(manager.unload("ObsPlugin"));
}

TEST_CASE("Observer: remove_observer stops notifications", "[observer]") {
  PluginManager manager;
  RecordingObserver obs;
  manager.add_observer(&obs);

  manager.add_plugin(std::make_shared<ObsPlugin>(),
                     make_entry("ObsPlugin", "obs"));
  CHECK(obs.events.size() == 1);

  manager.remove_observer(&obs);
  manager.unload("ObsPlugin");
  CHECK(obs.events.size() == 1);  // no new events
}

TEST_CASE("Observer: multiple observers", "[observer]") {
  PluginManager manager;
  RecordingObserver obs1, obs2;
  manager.add_observer(&obs1);
  manager.add_observer(&obs2);

  manager.add_plugin(std::make_shared<ObsPlugin>(),
                     make_entry("ObsPlugin", "obs"));

  CHECK(obs1.events.size() == 1);
  CHECK(obs2.events.size() == 1);

  manager.shutdown();
}

TEST_CASE("Observer: throwing observer does not break manager", "[observer]") {
  class ThrowingObserver : public PluginObserver {
   public:
    void on_plugin_loaded(const std::string&, const std::string&, const PluginEntry&) override {
      throw std::runtime_error("boom");
    }
    void on_plugin_unloaded(const std::string&, const std::string&, const PluginEntry&) override {
      throw std::runtime_error("boom");
    }
    void on_plugin_disabled(const std::string&, const std::string&, const PluginEntry&) override {
      throw std::runtime_error("boom");
    }
    void on_plugin_enabled(const std::string&, const std::string&, const PluginEntry&) override {
      throw std::runtime_error("boom");
    }
  };

  PluginManager manager;
  ThrowingObserver bad;
  RecordingObserver good;
  manager.add_observer(&bad);
  manager.add_observer(&good);

  // Throwing observer must not prevent the second observer from firing,
  // and must not break the manager's state.
  CHECK_NOTHROW(manager.add_plugin(std::make_shared<ObsPlugin>(),
                                   make_entry("ObsPlugin", "obs")));
  CHECK(good.events.size() == 1);
  CHECK(good.events[0].action == "loaded");

  CHECK_NOTHROW(manager.disable("ObsPlugin"));
  CHECK(good.events.size() == 2);
  CHECK(good.events[1].action == "disabled");

  CHECK_NOTHROW(manager.enable("ObsPlugin"));
  CHECK(good.events.size() == 3);
  CHECK(good.events[2].action == "enabled");

  CHECK_NOTHROW(manager.unload("ObsPlugin"));
  CHECK(good.events.size() == 4);
  CHECK(good.events[3].action == "unloaded");
}

TEST_CASE("Observer: add_observer with nullptr is ignored", "[observer]") {
  PluginManager manager;
  manager.add_observer(nullptr);

  CHECK_NOTHROW(manager.add_plugin(std::make_shared<ObsPlugin>(),
                                   make_entry("ObsPlugin", "obs")));
  manager.shutdown();
}
