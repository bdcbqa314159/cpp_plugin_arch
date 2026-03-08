#include <catch2/catch_test_macros.hpp>
#include <string>

#include "IHealthAware.hpp"
#include "ILifecycleAware.hpp"
#include "PluginManager.hpp"

using namespace plugin_arch;

// --- Test plugins ---

class HealthyPlugin : public IPlugin, public IHealthAware {
 public:
  const std::string& name() const override {
    static const std::string n = "HealthyPlugin";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "healthy";
    return t;
  }
  bool is_healthy() const override { return true; }
  HealthStatus health_status() const override { return {true, ""}; }
};

class UnhealthyPlugin : public IPlugin, public IHealthAware {
 public:
  const std::string& name() const override {
    static const std::string n = "UnhealthyPlugin";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "unhealthy";
    return t;
  }
  bool is_healthy() const override { return false; }
  HealthStatus health_status() const override {
    return {false, "disk full"};
  }
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

TEST_CASE("HealthCheck: no plugins returns empty", "[health]") {
  PluginManager manager;
  auto reports = manager.check_health();
  CHECK(reports.empty());
}

TEST_CASE("HealthCheck: healthy plugin excluded by default", "[health]") {
  PluginManager manager;
  manager.add_plugin(std::make_shared<HealthyPlugin>(),
                     make_entry("HealthyPlugin", "healthy"));

  auto reports = manager.check_health();
  CHECK(reports.empty());

  manager.shutdown();
}

TEST_CASE("HealthCheck: healthy plugin included when requested", "[health]") {
  PluginManager manager;
  manager.add_plugin(std::make_shared<HealthyPlugin>(),
                     make_entry("HealthyPlugin", "healthy"));

  auto reports = manager.check_health(/*include_healthy=*/true);
  REQUIRE(reports.size() == 1);
  CHECK(reports[0].plugin_name == "HealthyPlugin");
  CHECK(reports[0].status.healthy);

  manager.shutdown();
}

TEST_CASE("HealthCheck: unhealthy plugin reported", "[health]") {
  PluginManager manager;
  manager.add_plugin(std::make_shared<UnhealthyPlugin>(),
                     make_entry("UnhealthyPlugin", "unhealthy"));

  auto reports = manager.check_health();
  REQUIRE(reports.size() == 1);
  CHECK(reports[0].plugin_name == "UnhealthyPlugin");
  CHECK_FALSE(reports[0].status.healthy);
  CHECK(reports[0].status.message == "disk full");

  manager.shutdown();
}

TEST_CASE("HealthCheck: plain plugin (no IHealthAware) is skipped",
          "[health]") {
  PluginManager manager;
  manager.add_plugin(std::make_shared<PlainPlugin>(),
                     make_entry("PlainPlugin", "plain"));

  auto reports = manager.check_health(/*include_healthy=*/true);
  CHECK(reports.empty());

  manager.shutdown();
}

TEST_CASE("HealthCheck: mixed plugins", "[health]") {
  PluginManager manager;
  manager.add_plugin(std::make_shared<HealthyPlugin>(),
                     make_entry("HealthyPlugin", "healthy"));
  manager.add_plugin(std::make_shared<UnhealthyPlugin>(),
                     make_entry("UnhealthyPlugin", "unhealthy"));
  manager.add_plugin(std::make_shared<PlainPlugin>(),
                     make_entry("PlainPlugin", "plain"));

  // Default: only unhealthy
  auto reports = manager.check_health();
  REQUIRE(reports.size() == 1);
  CHECK(reports[0].plugin_name == "UnhealthyPlugin");

  // Include healthy: healthy + unhealthy (plain is not IHealthAware)
  auto all = manager.check_health(true);
  CHECK(all.size() == 2);

  manager.shutdown();
}

TEST_CASE("HealthCheck: disabled plugins are skipped", "[health]") {
  PluginManager manager;
  manager.add_plugin(std::make_shared<UnhealthyPlugin>(),
                     make_entry("UnhealthyPlugin", "unhealthy"));

  manager.disable("UnhealthyPlugin");

  auto reports = manager.check_health();
  CHECK(reports.empty());

  manager.shutdown();
}
