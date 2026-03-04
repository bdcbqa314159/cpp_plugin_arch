#include <catch2/catch_test_macros.hpp>

#include "IPlugin.hpp"
#include "ServiceLocator.hpp"

using namespace plugin_arch;

class LocatorTestPlugin : public IPlugin {
 public:
  explicit LocatorTestPlugin(std::string type) : type_(std::move(type)) {}
  const std::string& name() const override { static const std::string s("TestPlugin"); return s; }
  const std::string& version() const override { static const std::string s("1.0"); return s; }
  const std::string& type() const override { return type_; }
 private:
  std::string type_;
};

TEST_CASE("ServiceLocator add and get", "[locator]") {
  ServiceLocator locator;
  auto plugin = std::make_shared<LocatorTestPlugin>("test_type");
  locator.add(plugin);

  auto found = locator.get<IPlugin>("test_type");
  CHECK(found != nullptr);
  CHECK(found->type() == "test_type");
}

TEST_CASE("ServiceLocator get returns nullptr for unknown type", "[locator]") {
  ServiceLocator locator;
  auto found = locator.get<IPlugin>("nonexistent");
  CHECK(found == nullptr);
}

TEST_CASE("ServiceLocator get_all returns matching services", "[locator]") {
  ServiceLocator locator;
  // Must keep shared_ptrs alive — locator stores weak_ptrs
  auto c1 = std::make_shared<LocatorTestPlugin>("calc");
  auto c2 = std::make_shared<LocatorTestPlugin>("calc");
  auto o1 = std::make_shared<LocatorTestPlugin>("other");
  locator.add(c1);
  locator.add(c2);
  locator.add(o1);

  auto calcs = locator.get_all<IPlugin>("calc");
  CHECK(calcs.size() == 2);

  auto others = locator.get_all<IPlugin>("other");
  CHECK(others.size() == 1);
}

TEST_CASE("ServiceLocator ignores nullptr", "[locator]") {
  ServiceLocator locator;
  locator.add(nullptr);
  CHECK(locator.size() == 0);
}

TEST_CASE("ServiceLocator cleanup removes expired entries", "[locator]") {
  ServiceLocator locator;
  {
    auto plugin = std::make_shared<LocatorTestPlugin>("temp");
    locator.add(plugin);
    CHECK(locator.size() == 1);
  }
  // plugin is now destroyed, weak_ptr expired
  locator.cleanup();
  CHECK(locator.size() == 0);
}

TEST_CASE("ServiceLocator clear removes all entries", "[locator]") {
  ServiceLocator locator;
  auto p1 = std::make_shared<LocatorTestPlugin>("a");
  auto p2 = std::make_shared<LocatorTestPlugin>("b");
  locator.add(p1);
  locator.add(p2);
  CHECK(locator.size() == 2);

  locator.clear();
  CHECK(locator.size() == 0);
}
