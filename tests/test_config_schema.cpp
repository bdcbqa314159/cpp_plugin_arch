#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

#include "IConfigSchema.hpp"
#include "IConfigurable.hpp"
#include "PluginManager.hpp"

using namespace plugin_arch;

// --- Test plugin with schema ---

class SchemaPlugin : public IPlugin,
                     public IConfigurable,
                     public IConfigSchema {
 public:
  const std::string& name() const override {
    static const std::string n = "SchemaPlugin";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "schema";
    return t;
  }

  void configure(const PluginConfig& config) override {
    config_ = config;
  }

  std::vector<ConfigKeyDef> config_schema() const override {
    return {
        {"host", true, "", "Database hostname"},
        {"port", false, "5432", "Database port"},
        {"timeout", false, "30", "Connection timeout in seconds"},
    };
  }

  PluginConfig config_;
};

// Plugin with schema but no IConfigurable (shouldn't be validated)
class NonConfigurableSchemaPlugin : public IPlugin, public IConfigSchema {
 public:
  const std::string& name() const override {
    static const std::string n = "NonConfigurable";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "nonconfig";
    return t;
  }
  std::vector<ConfigKeyDef> config_schema() const override {
    return {{"key", true, "", "required"}};
  }
};

// Plugin with IConfigurable but no IConfigSchema (backwards compatible)
class NoSchemaPlugin : public IPlugin, public IConfigurable {
 public:
  const std::string& name() const override {
    static const std::string n = "NoSchemaPlugin";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "noschema";
    return t;
  }
  void configure(const PluginConfig& config) override {
    config_ = config;
  }
  PluginConfig config_;
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

TEST_CASE("Config schema: validate_config with all keys provided",
          "[config][schema]") {
  SchemaPlugin plugin;
  PluginConfig config = {{"host", "localhost"}, {"port", "3306"}};

  auto errors = PluginManager::validate_config(&plugin, config);
  CHECK(errors.empty());
  // "timeout" should get default applied
  CHECK(config["timeout"] == "30");
}

TEST_CASE("Config schema: missing required key", "[config][schema]") {
  SchemaPlugin plugin;
  PluginConfig config = {{"port", "3306"}};  // missing "host"

  auto errors = PluginManager::validate_config(&plugin, config);
  REQUIRE(errors.size() == 1);
  CHECK(errors[0] == "Missing required config key: host");
}

TEST_CASE("Config schema: defaults applied for optional keys",
          "[config][schema]") {
  SchemaPlugin plugin;
  PluginConfig config = {{"host", "db.example.com"}};

  auto errors = PluginManager::validate_config(&plugin, config);
  CHECK(errors.empty());
  CHECK(config["port"] == "5432");
  CHECK(config["timeout"] == "30");
}

TEST_CASE("Config schema: no schema = always valid", "[config][schema]") {
  NoSchemaPlugin plugin;
  PluginConfig config = {{"anything", "goes"}};

  auto errors = PluginManager::validate_config(&plugin, config);
  CHECK(errors.empty());
}

TEST_CASE("Config schema: add_plugin throws on missing required key",
          "[config][schema]") {
  PluginManager manager;

  // No config provided, but "host" is required
  CHECK_THROWS_AS(
      manager.add_plugin(std::make_shared<SchemaPlugin>(),
                         make_entry("SchemaPlugin", "schema")),
      std::runtime_error);

  CHECK_FALSE(manager.is_loaded("SchemaPlugin"));
}

TEST_CASE("Config schema: add_plugin succeeds with valid config",
          "[config][schema]") {
  PluginManager manager;
  auto plugin = std::make_shared<SchemaPlugin>();

  PluginManager::ConfigMap config = {
      {"SchemaPlugin", {{"host", "localhost"}}}};

  CHECK_NOTHROW(manager.add_plugin(plugin, make_entry("SchemaPlugin", "schema"),
                                   config));

  // Defaults should have been applied
  CHECK(plugin->config_["host"] == "localhost");
  CHECK(plugin->config_["port"] == "5432");
  CHECK(plugin->config_["timeout"] == "30");

  manager.shutdown();
}

TEST_CASE("Config schema: backward compat — no schema, no config",
          "[config][schema]") {
  PluginManager manager;

  // NoSchemaPlugin has IConfigurable but no IConfigSchema — should work fine
  CHECK_NOTHROW(
      manager.add_plugin(std::make_shared<NoSchemaPlugin>(),
                         make_entry("NoSchemaPlugin", "noschema")));

  manager.shutdown();
}

TEST_CASE("Config schema: empty config with all optional keys",
          "[config][schema]") {
  // A plugin where all keys are optional with defaults
  class AllOptionalPlugin : public IPlugin,
                            public IConfigurable,
                            public IConfigSchema {
   public:
    const std::string& name() const override {
      static const std::string n = "AllOptional";
      return n;
    }
    const std::string& version() const override {
      static const std::string v = "1.0.0";
      return v;
    }
    const std::string& type() const override {
      static const std::string t = "allopt";
      return t;
    }
    void configure(const PluginConfig& config) override { config_ = config; }
    std::vector<ConfigKeyDef> config_schema() const override {
      return {{"level", false, "info", "Log level"}};
    }
    PluginConfig config_;
  };

  PluginManager manager;
  auto plugin = std::make_shared<AllOptionalPlugin>();
  manager.add_plugin(plugin, make_entry("AllOptional", "allopt"));

  CHECK(plugin->config_["level"] == "info");

  manager.shutdown();
}
