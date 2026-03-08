#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
#include <string>
#include <vector>

#include "EventBus.hpp"

using namespace plugin_arch;

TEST_CASE("Vetoable: no subscribers means event is allowed",
          "[eventbus][veto]") {
  EventBus bus;
  CHECK(bus.publish_vetoable("test", "payload"));
}

TEST_CASE("Vetoable: all handlers allow", "[eventbus][veto]") {
  EventBus bus;
  (void)bus.subscribe_vetoable(
      "test", [](const std::string&, const std::string&) { return true; });
  (void)bus.subscribe_vetoable(
      "test", [](const std::string&, const std::string&) { return true; });

  CHECK(bus.publish_vetoable("test"));
}

TEST_CASE("Vetoable: single handler vetoes", "[eventbus][veto]") {
  EventBus bus;
  (void)bus.subscribe_vetoable(
      "test", [](const std::string&, const std::string&) { return false; });

  CHECK_FALSE(bus.publish_vetoable("test"));
}

TEST_CASE("Vetoable: stop_on_veto=true skips remaining handlers",
          "[eventbus][veto]") {
  EventBus bus;
  int call_count = 0;

  (void)bus.subscribe_vetoable(
      "test",
      [&](const std::string&, const std::string&) {
        ++call_count;
        return false;  // veto
      },
      10);  // high priority, called first

  (void)bus.subscribe_vetoable(
      "test",
      [&](const std::string&, const std::string&) {
        ++call_count;
        return true;
      },
      0);  // lower priority

  CHECK_FALSE(bus.publish_vetoable("test", "", /*stop_on_veto=*/true));
  CHECK(call_count == 1);  // second handler was skipped
}

TEST_CASE("Vetoable: stop_on_veto=false calls all handlers",
          "[eventbus][veto]") {
  EventBus bus;
  int call_count = 0;

  (void)bus.subscribe_vetoable(
      "test",
      [&](const std::string&, const std::string&) {
        ++call_count;
        return false;  // veto
      },
      10);

  (void)bus.subscribe_vetoable(
      "test",
      [&](const std::string&, const std::string&) {
        ++call_count;
        return true;
      },
      0);

  CHECK_FALSE(bus.publish_vetoable("test", "", /*stop_on_veto=*/false));
  CHECK(call_count == 2);  // both handlers called
}

TEST_CASE("Vetoable: respects priority ordering", "[eventbus][veto]") {
  EventBus bus;
  std::vector<int> order;

  (void)bus.subscribe_vetoable(
      "test",
      [&](const std::string&, const std::string&) {
        order.push_back(1);
        return true;
      },
      1);

  (void)bus.subscribe_vetoable(
      "test",
      [&](const std::string&, const std::string&) {
        order.push_back(3);
        return true;
      },
      3);

  (void)bus.publish_vetoable("test");

  REQUIRE(order.size() == 2);
  CHECK(order[0] == 3);
  CHECK(order[1] == 1);
}

TEST_CASE("Vetoable: throwing handler does not block others",
          "[eventbus][veto]") {
  EventBus bus;
  int call_count = 0;

  (void)bus.subscribe_vetoable(
      "test",
      [](const std::string&, const std::string&) -> bool {
        throw std::runtime_error("boom");
      },
      10);

  (void)bus.subscribe_vetoable(
      "test",
      [&](const std::string&, const std::string&) {
        ++call_count;
        return true;
      },
      0);

  CHECK(bus.publish_vetoable("test", "", /*stop_on_veto=*/false));
  CHECK(call_count == 1);
}

TEST_CASE("Vetoable: unsubscribe works", "[eventbus][veto]") {
  EventBus bus;
  auto id = bus.subscribe_vetoable(
      "test", [](const std::string&, const std::string&) { return false; });

  CHECK_FALSE(bus.publish_vetoable("test"));

  CHECK(bus.unsubscribe(id));
  CHECK(bus.publish_vetoable("test"));  // no more veto
}

TEST_CASE("Vetoable: null handler throws", "[eventbus][veto]") {
  EventBus bus;
  EventBus::VetoHandler null_handler;
  CHECK_THROWS_AS(bus.subscribe_vetoable("test", null_handler),
                  std::invalid_argument);
}

TEST_CASE("Vetoable: vetoable_subscriber_count", "[eventbus][veto]") {
  EventBus bus;
  CHECK(bus.vetoable_subscriber_count("test") == 0);

  (void)bus.subscribe_vetoable(
      "test", [](const std::string&, const std::string&) { return true; });
  CHECK(bus.vetoable_subscriber_count("test") == 1);

  (void)bus.subscribe_vetoable(
      "test", [](const std::string&, const std::string&) { return true; });
  CHECK(bus.vetoable_subscriber_count("test") == 2);
}

TEST_CASE("Vetoable: subscriber_count includes vetoable",
          "[eventbus][veto]") {
  EventBus bus;

  (void)bus.subscribe("test",
                      [](const std::string&, const std::string&) {});
  (void)bus.subscribe_vetoable(
      "test", [](const std::string&, const std::string&) { return true; });

  CHECK(bus.subscriber_count("test") == 2);
}

TEST_CASE("Vetoable: payload is passed to handler", "[eventbus][veto]") {
  EventBus bus;
  std::string received;

  (void)bus.subscribe_vetoable(
      "test", [&](const std::string&, const std::string& payload) {
        received = payload;
        return true;
      });

  (void)bus.publish_vetoable("test", "hello");
  CHECK(received == "hello");
}
