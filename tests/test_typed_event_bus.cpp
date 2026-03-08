#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
#include <string>
#include <vector>

#include "EventBus.hpp"

using namespace plugin_arch;

// A structured event type for testing.
struct UserEvent {
  std::string user;
  int action;
};

TEST_CASE("Typed EventBus: subscribe and publish", "[eventbus][typed]") {
  EventBus bus;
  UserEvent received{};

  (void)bus.subscribe_typed<UserEvent>(
      "user", [&](const std::string&, const UserEvent& e) { received = e; });

  bus.publish_typed<UserEvent>("user", {"alice", 42});
  CHECK(received.user == "alice");
  CHECK(received.action == 42);
}

TEST_CASE("Typed EventBus: multiple subscribers", "[eventbus][typed]") {
  EventBus bus;
  int count = 0;

  (void)bus.subscribe_typed<int>("tick",
                                 [&](const std::string&, const int&) { ++count; });
  (void)bus.subscribe_typed<int>("tick",
                                 [&](const std::string&, const int&) { ++count; });

  bus.publish_typed<int>("tick", 1);
  CHECK(count == 2);
}

TEST_CASE("Typed EventBus: unsubscribe", "[eventbus][typed]") {
  EventBus bus;
  int count = 0;

  auto id = bus.subscribe_typed<int>(
      "test", [&](const std::string&, const int&) { ++count; });

  bus.publish_typed<int>("test", 1);
  CHECK(count == 1);

  CHECK(bus.unsubscribe(id));
  bus.publish_typed<int>("test", 2);
  CHECK(count == 1);  // handler was removed
}

TEST_CASE("Typed EventBus: null handler throws", "[eventbus][typed]") {
  EventBus bus;
  EventBus::TypedHandler<int> null_handler;
  CHECK_THROWS_AS(bus.subscribe_typed<int>("test", null_handler),
                  std::invalid_argument);
}

TEST_CASE("Typed EventBus: publish to nonexistent topic is no-op",
          "[eventbus][typed]") {
  EventBus bus;
  CHECK_NOTHROW(bus.publish_typed<int>("nonexistent", 42));
}

TEST_CASE("Typed EventBus: typed_subscriber_count", "[eventbus][typed]") {
  EventBus bus;
  CHECK(bus.typed_subscriber_count("test") == 0);

  (void)bus.subscribe_typed<int>(
      "test", [](const std::string&, const int&) {});
  CHECK(bus.typed_subscriber_count("test") == 1);
}

TEST_CASE("Typed EventBus: subscriber_count includes both kinds",
          "[eventbus][typed]") {
  EventBus bus;

  (void)bus.subscribe("test",
                      [](const std::string&, const std::string&) {});
  (void)bus.subscribe_typed<int>(
      "test", [](const std::string&, const int&) {});

  CHECK(bus.subscriber_count("test") == 2);
  CHECK(bus.typed_subscriber_count("test") == 1);
}

TEST_CASE("Typed EventBus: string and typed are independent",
          "[eventbus][typed]") {
  EventBus bus;
  bool string_fired = false;
  bool typed_fired = false;

  (void)bus.subscribe("test", [&](const std::string&, const std::string&) {
    string_fired = true;
  });
  (void)bus.subscribe_typed<int>(
      "test", [&](const std::string&, const int&) { typed_fired = true; });

  // String publish only fires string handlers
  bus.publish("test", "hello");
  CHECK(string_fired);
  CHECK_FALSE(typed_fired);

  string_fired = false;

  // Typed publish only fires typed handlers
  bus.publish_typed<int>("test", 42);
  CHECK_FALSE(string_fired);
  CHECK(typed_fired);
}

TEST_CASE("Typed EventBus: throwing handler does not block others",
          "[eventbus][typed]") {
  EventBus bus;
  int count = 0;

  (void)bus.subscribe_typed<int>(
      "test", [](const std::string&, const int&) {
        throw std::runtime_error("boom");
      });
  (void)bus.subscribe_typed<int>(
      "test", [&](const std::string&, const int&) { ++count; });

  bus.publish_typed<int>("test", 1);
  CHECK(count == 1);
}

TEST_CASE("Typed EventBus: clear removes typed subscriptions",
          "[eventbus][typed]") {
  EventBus bus;
  int count = 0;

  (void)bus.subscribe_typed<int>(
      "test", [&](const std::string&, const int&) { ++count; });

  bus.clear();
  bus.publish_typed<int>("test", 1);
  CHECK(count == 0);
}

TEST_CASE("Typed EventBus: complex event type", "[eventbus][typed]") {
  EventBus bus;

  struct MetricsEvent {
    std::string name;
    double value;
    std::vector<std::string> tags;
  };

  MetricsEvent received;
  (void)bus.subscribe_typed<MetricsEvent>(
      "metrics",
      [&](const std::string&, const MetricsEvent& e) { received = e; });

  bus.publish_typed<MetricsEvent>("metrics",
                                  {"cpu_usage", 0.75, {"host:a", "env:prod"}});
  CHECK(received.name == "cpu_usage");
  CHECK(received.value == 0.75);
  REQUIRE(received.tags.size() == 2);
  CHECK(received.tags[0] == "host:a");
  CHECK(received.tags[1] == "env:prod");
}
