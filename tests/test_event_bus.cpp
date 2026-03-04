#include <catch2/catch_test_macros.hpp>

#include "EventBus.hpp"

using namespace plugin_arch;

TEST_CASE("EventBus subscribe and publish", "[eventbus]") {
  EventBus bus;
  std::string received;

  (void)bus.subscribe("test", [&](const std::string&, const std::string& payload) {
    received = payload;
  });

  bus.publish("test", "hello");
  CHECK(received == "hello");
}

TEST_CASE("EventBus multiple subscribers", "[eventbus]") {
  EventBus bus;
  int count = 0;

  (void)bus.subscribe("tick", [&](const std::string&, const std::string&) { ++count; });
  (void)bus.subscribe("tick", [&](const std::string&, const std::string&) { ++count; });

  bus.publish("tick");
  CHECK(count == 2);
}

TEST_CASE("EventBus unsubscribe", "[eventbus]") {
  EventBus bus;
  int count = 0;

  auto id = bus.subscribe("test", [&](const std::string&, const std::string&) {
    ++count;
  });

  bus.publish("test");
  CHECK(count == 1);

  CHECK(bus.unsubscribe(id));
  bus.publish("test");
  CHECK(count == 1);  // handler was removed
}

TEST_CASE("EventBus unsubscribe returns false for unknown id", "[eventbus]") {
  EventBus bus;
  CHECK_FALSE(bus.unsubscribe(42));
}

TEST_CASE("EventBus publish to nonexistent topic is no-op", "[eventbus]") {
  EventBus bus;
  CHECK_NOTHROW(bus.publish("nonexistent", "data"));
}

TEST_CASE("EventBus subscriber_count", "[eventbus]") {
  EventBus bus;
  CHECK(bus.subscriber_count("test") == 0);

  (void)bus.subscribe("test", [](const std::string&, const std::string&) {});
  CHECK(bus.subscriber_count("test") == 1);

  (void)bus.subscribe("test", [](const std::string&, const std::string&) {});
  CHECK(bus.subscriber_count("test") == 2);
}

TEST_CASE("EventBus clear removes all subscriptions", "[eventbus]") {
  EventBus bus;
  int count = 0;

  (void)bus.subscribe("a", [&](const std::string&, const std::string&) { ++count; });
  (void)bus.subscribe("b", [&](const std::string&, const std::string&) { ++count; });

  bus.clear();
  bus.publish("a");
  bus.publish("b");
  CHECK(count == 0);
}
