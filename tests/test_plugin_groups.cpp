#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

#include "ILifecycleAware.hpp"
#include "IPluginMetadata.hpp"
#include "PluginManager.hpp"

using namespace plugin_arch;

// --- Test plugins with capabilities ---

class StoragePlugin : public IPlugin,
                      public ILifecycleAware,
                      public IPluginMetadata {
 public:
  StoragePlugin(std::string name, std::string type)
      : name_(std::move(name)), type_(std::move(type)) {}

  const std::string& name() const override { return name_; }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override { return type_; }

  void on_init() override { ++init_count; }
  void on_shutdown() override { ++shutdown_count; }

  const std::string& author() const override {
    static const std::string a = "test";
    return a;
  }
  const std::string& description() const override {
    static const std::string d = "test storage";
    return d;
  }
  const std::string& license() const override {
    static const std::string l = "MIT";
    return l;
  }
  std::vector<std::string> capabilities() const override {
    return {"storage", "io"};
  }

  int init_count = 0;
  int shutdown_count = 0;

 private:
  std::string name_;
  std::string type_;
};

class NetworkPlugin : public IPlugin,
                      public ILifecycleAware,
                      public IPluginMetadata {
 public:
  const std::string& name() const override {
    static const std::string n = "NetworkPlugin";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "network";
    return t;
  }

  void on_init() override { ++init_count; }
  void on_shutdown() override { ++shutdown_count; }

  const std::string& author() const override {
    static const std::string a = "test";
    return a;
  }
  const std::string& description() const override {
    static const std::string d = "network";
    return d;
  }
  const std::string& license() const override {
    static const std::string l = "MIT";
    return l;
  }
  std::vector<std::string> capabilities() const override {
    return {"network", "io"};
  }

  int init_count = 0;
  int shutdown_count = 0;
};

class PlainPlugin : public IPlugin {
 public:
  const std::string& name() const override {
    static const std::string n = "PlainPlugin";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "plain";
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

// --- Tests ---

TEST_CASE("Plugin groups: plugins_with_capability", "[groups]") {
  PluginManager manager;
  manager.add_plugin(std::make_shared<StoragePlugin>("FileStore", "filestore"),
                     make_entry("FileStore", "filestore"));
  manager.add_plugin(std::make_shared<NetworkPlugin>(),
                     make_entry("NetworkPlugin", "network"));
  manager.add_plugin(std::make_shared<PlainPlugin>(),
                     make_entry("PlainPlugin", "plain"));

  auto io_plugins = manager.plugins_with_capability("io");
  CHECK(io_plugins.size() == 2);

  auto storage_plugins = manager.plugins_with_capability("storage");
  REQUIRE(storage_plugins.size() == 1);
  CHECK(storage_plugins[0] == "FileStore");

  auto network_plugins = manager.plugins_with_capability("network");
  REQUIRE(network_plugins.size() == 1);
  CHECK(network_plugins[0] == "NetworkPlugin");

  auto none = manager.plugins_with_capability("nonexistent");
  CHECK(none.empty());

  manager.shutdown();
}

TEST_CASE("Plugin groups: disable_group", "[groups]") {
  PluginManager manager;
  auto store = std::make_shared<StoragePlugin>("FileStore", "filestore");
  auto net = std::make_shared<NetworkPlugin>();

  manager.add_plugin(store, make_entry("FileStore", "filestore"));
  manager.add_plugin(net, make_entry("NetworkPlugin", "network"));

  CHECK(manager.is_enabled("FileStore"));
  CHECK(manager.is_enabled("NetworkPlugin"));

  // Disable all "io" plugins — both have this capability
  manager.disable_group("io");

  CHECK_FALSE(manager.is_enabled("FileStore"));
  CHECK_FALSE(manager.is_enabled("NetworkPlugin"));

  manager.shutdown();
}

TEST_CASE("Plugin groups: enable_group", "[groups]") {
  PluginManager manager;
  auto store = std::make_shared<StoragePlugin>("FileStore", "filestore");
  auto net = std::make_shared<NetworkPlugin>();

  manager.add_plugin(store, make_entry("FileStore", "filestore"));
  manager.add_plugin(net, make_entry("NetworkPlugin", "network"));

  manager.disable_group("io");
  CHECK_FALSE(manager.is_enabled("FileStore"));
  CHECK_FALSE(manager.is_enabled("NetworkPlugin"));

  manager.enable_group("io");
  CHECK(manager.is_enabled("FileStore"));
  CHECK(manager.is_enabled("NetworkPlugin"));

  manager.shutdown();
}

TEST_CASE("Plugin groups: unload_group", "[groups]") {
  PluginManager manager;
  manager.add_plugin(std::make_shared<StoragePlugin>("FileStore", "filestore"),
                     make_entry("FileStore", "filestore"));
  manager.add_plugin(std::make_shared<NetworkPlugin>(),
                     make_entry("NetworkPlugin", "network"));
  manager.add_plugin(std::make_shared<PlainPlugin>(),
                     make_entry("PlainPlugin", "plain"));

  CHECK(manager.instances().size() == 3);

  // Unload only "storage" group
  manager.unload_group("storage");

  CHECK_FALSE(manager.is_loaded("FileStore"));
  CHECK(manager.is_loaded("NetworkPlugin"));
  CHECK(manager.is_loaded("PlainPlugin"));

  manager.shutdown();
}

TEST_CASE("Plugin groups: disable_group selective capability", "[groups]") {
  PluginManager manager;
  auto store = std::make_shared<StoragePlugin>("FileStore", "filestore");
  auto net = std::make_shared<NetworkPlugin>();

  manager.add_plugin(store, make_entry("FileStore", "filestore"));
  manager.add_plugin(net, make_entry("NetworkPlugin", "network"));

  // Only disable "storage" — NetworkPlugin has "io" but not "storage"
  manager.disable_group("storage");

  CHECK_FALSE(manager.is_enabled("FileStore"));
  CHECK(manager.is_enabled("NetworkPlugin"));

  manager.shutdown();
}
