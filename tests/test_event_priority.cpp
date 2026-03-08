#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

#include "EventBus.hpp"

using namespace plugin_arch;

TEST_CASE("EventBus priority: higher priority dispatched first",
          "[eventbus][priority]") {
  EventBus bus;
  std::vector<int> order;

  (void)bus.subscribe(
      "test", [&](const std::string&, const std::string&) { order.push_back(1); },
      /*priority=*/1);
  (void)bus.subscribe(
      "test", [&](const std::string&, const std::string&) { order.push_back(3); },
      /*priority=*/3);
  (void)bus.subscribe(
      "test", [&](const std::string&, const std::string&) { order.push_back(2); },
      /*priority=*/2);

  bus.publish("test");

  REQUIRE(order.size() == 3);
  CHECK(order[0] == 3);
  CHECK(order[1] == 2);
  CHECK(order[2] == 1);
}

TEST_CASE("EventBus priority: same priority preserves insertion order",
          "[eventbus][priority]") {
  EventBus bus;
  std::vector<char> order;

  (void)bus.subscribe(
      "test", [&](const std::string&, const std::string&) { order.push_back('A'); },
      0);
  (void)bus.subscribe(
      "test", [&](const std::string&, const std::string&) { order.push_back('B'); },
      0);
  (void)bus.subscribe(
      "test", [&](const std::string&, const std::string&) { order.push_back('C'); },
      0);

  bus.publish("test");

  REQUIRE(order.size() == 3);
  CHECK(order[0] == 'A');
  CHECK(order[1] == 'B');
  CHECK(order[2] == 'C');
}

TEST_CASE("EventBus priority: default priority is 0", "[eventbus][priority]") {
  EventBus bus;
  std::vector<int> order;

  (void)bus.subscribe(
      "test",
      [&](const std::string&, const std::string&) { order.push_back(0); });
  (void)bus.subscribe(
      "test", [&](const std::string&, const std::string&) { order.push_back(10); },
      10);

  bus.publish("test");

  REQUIRE(order.size() == 2);
  CHECK(order[0] == 10);  // higher priority first
  CHECK(order[1] == 0);
}

TEST_CASE("EventBus priority: negative priorities are valid",
          "[eventbus][priority]") {
  EventBus bus;
  std::vector<int> order;

  (void)bus.subscribe(
      "test", [&](const std::string&, const std::string&) { order.push_back(-5); },
      -5);
  (void)bus.subscribe(
      "test", [&](const std::string&, const std::string&) { order.push_back(0); },
      0);
  (void)bus.subscribe(
      "test", [&](const std::string&, const std::string&) { order.push_back(5); },
      5);

  bus.publish("test");

  REQUIRE(order.size() == 3);
  CHECK(order[0] == 5);
  CHECK(order[1] == 0);
  CHECK(order[2] == -5);
}

TEST_CASE("EventBus priority: typed handlers respect priority",
          "[eventbus][priority]") {
  EventBus bus;
  std::vector<int> order;

  (void)bus.subscribe_typed<int>(
      "test", [&](const std::string&, const int&) { order.push_back(1); }, 1);
  (void)bus.subscribe_typed<int>(
      "test", [&](const std::string&, const int&) { order.push_back(3); }, 3);
  (void)bus.subscribe_typed<int>(
      "test", [&](const std::string&, const int&) { order.push_back(2); }, 2);

  bus.publish_typed<int>("test", 42);

  REQUIRE(order.size() == 3);
  CHECK(order[0] == 3);
  CHECK(order[1] == 2);
  CHECK(order[2] == 1);
}
