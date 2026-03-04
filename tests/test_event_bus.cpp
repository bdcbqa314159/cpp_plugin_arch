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

TEST_CASE("EventBus subscribe during publish is safe", "[eventbus]") {
  EventBus bus;
  int count = 0;

  (void)bus.subscribe("test", [&](const std::string&, const std::string&) {
    ++count;
    // Subscribe during publish — must not invalidate iteration
    (void)bus.subscribe("test", [&](const std::string&, const std::string&) {
      ++count;
    });
  });

  bus.publish("test");
  // Only the original handler fires (snapshot taken before iteration)
  CHECK(count == 1);

  // Second publish: both handlers fire
  bus.publish("test");
  CHECK(count == 3);
}

TEST_CASE("EventBus unsubscribe during publish is safe", "[eventbus]") {
  EventBus bus;
  int count = 0;
  EventBus::SubscriptionId id2 = EventBus::invalid_id;

  (void)bus.subscribe("test", [&](const std::string&, const std::string&) {
    ++count;
    // Unsubscribe the other handler during publish
    bus.unsubscribe(id2);
  });

  id2 = bus.subscribe("test", [&](const std::string&, const std::string&) {
    ++count;
  });

  bus.publish("test");
  // Both fire (snapshot was taken before the unsubscribe)
  CHECK(count == 2);

  // Second publish: only the first handler fires (id2 was unsubscribed)
  bus.publish("test");
  CHECK(count == 3);
}

TEST_CASE("EventBus subscribe with null handler returns invalid_id", "[eventbus]") {
  EventBus bus;
  EventBus::Handler null_handler;
  auto id = bus.subscribe("test", null_handler);
  CHECK(id == EventBus::invalid_id);
  CHECK(bus.subscriber_count("test") == 0);
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
