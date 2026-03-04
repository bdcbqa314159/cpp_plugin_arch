#include <catch2/catch_test_macros.hpp>

#include "IPlugin.hpp"
#include "MockPluginLoader.hpp"

using namespace plugin_arch;

// Minimal concrete plugin for testing
class TestPlugin : public IPlugin {
 public:
  const std::string& name() const override { static const std::string s("Test"); return s; }
  const std::string& version() const override { static const std::string s("1.0"); return s; }
  const std::string& type() const override { static const std::string s("test"); return s; }
};

TEST_CASE("MockPluginLoader returns provided instance", "[mock]") {
  auto instance = std::make_shared<TestPlugin>();
  MockPluginLoader<IPlugin> loader(instance);

  auto result = loader.get_instance();
  CHECK(result.get() == instance.get());
  CHECK(result->name() == "Test");
}

TEST_CASE("MockPluginLoader has default path 'mock'", "[mock]") {
  MockPluginLoader<IPlugin> loader(std::make_shared<TestPlugin>());
  CHECK(loader.path() == "mock");
}

TEST_CASE("MockPluginLoader accepts custom path", "[mock]") {
  MockPluginLoader<IPlugin> loader(std::make_shared<TestPlugin>(), "/custom/path.so");
  CHECK(loader.path() == "/custom/path.so");
}

TEST_CASE("MockHotPluginLoader returns instance and simulates reload", "[mock]") {
  auto v1 = std::make_shared<TestPlugin>();
  MockHotPluginLoader<IPlugin> loader(v1);

  CHECK(loader.get_instance().get() == v1.get());
  CHECK_FALSE(loader.check_and_reload());

  auto v2 = std::make_shared<TestPlugin>();
  loader.set_instance(v2);
  CHECK(loader.get_instance().get() == v2.get());
}
