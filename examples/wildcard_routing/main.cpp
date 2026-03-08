// Wildcard event routing — shows how a host subscribes to multiple
// topics in a loop, or uses a dispatcher plugin to fan out events.
//
// Pattern: subscribe to topics by prefix convention, use a central
// dispatcher to route events to the right handlers.

#include <iostream>
#include <string>
#include <vector>

#include "EventBus.hpp"
#include "ILifecycleAware.hpp"
#include "PluginManager.hpp"

using namespace plugin_arch;

// --- Wildcard subscriber (host-side code) ---

// Subscribe a handler to all topics matching a prefix.
// Returns the subscription IDs so the host can unsubscribe later.
static std::vector<EventBus::SubscriptionId> subscribe_prefix(
    EventBus& bus, const std::string& prefix,
    const std::vector<std::string>& all_topics, EventBus::Handler handler) {
  std::vector<EventBus::SubscriptionId> ids;
  for (const auto& topic : all_topics) {
    if (topic.compare(0, prefix.size(), prefix) == 0) {
      ids.push_back(bus.subscribe(topic, handler));
    }
  }
  return ids;
}

// --- Dispatcher plugin (host-side code) ---
// Routes all events from source topics to a single summary topic.

class EventDispatcher {
 public:
  EventDispatcher(EventBus& bus, const std::string& summary_topic)
      : bus_(bus), summary_topic_(summary_topic) {}

  void watch(const std::string& topic) {
    auto id = bus_.subscribe(
        topic, [this](const std::string& topic, const std::string& payload) {
          std::string summary = "[" + topic + "] " + payload;
          bus_.publish(summary_topic_, summary);
        });
    ids_.push_back(id);
  }

  void unwatch_all() {
    for (auto id : ids_) bus_.unsubscribe(id);
    ids_.clear();
  }

 private:
  EventBus& bus_;
  std::string summary_topic_;
  std::vector<EventBus::SubscriptionId> ids_;
};

// --- Demo plugins ---

class OrderPlugin : public IPlugin, public ILifecycleAware {
 public:
  const std::string& name() const override {
    static const std::string n = "OrderPlugin";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "order";
    return t;
  }
  void on_init() override {}
  void on_shutdown() override {}
};

class PaymentPlugin : public IPlugin, public ILifecycleAware {
 public:
  const std::string& name() const override {
    static const std::string n = "PaymentPlugin";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "payment";
    return t;
  }
  void on_init() override {}
  void on_shutdown() override {}
};

static PluginEntry make_entry(const std::string& name,
                              const std::string& type) {
  PluginEntry e;
  e.name = name;
  e.type = type;
  e.version = "1.0.0";
  return e;
}

// --- Main ---

int main() {
  PluginManager manager;
  EventBus bus;

  manager.add_plugin(std::make_shared<OrderPlugin>(),
                     make_entry("OrderPlugin", "order"));
  manager.add_plugin(std::make_shared<PaymentPlugin>(),
                     make_entry("PaymentPlugin", "payment"));

  // Define known topics
  std::vector<std::string> all_topics = {
      "order.created", "order.shipped", "order.cancelled",
      "payment.charged", "payment.refunded",
      "user.login", "user.logout"};

  // 1. Prefix-based wildcard subscription
  std::cout << "=== Prefix wildcard: order.* ===\n";
  auto order_ids = subscribe_prefix(
      bus, "order.",
      all_topics,
      [](const std::string& topic, const std::string& payload) {
        std::cout << "  [order handler] " << topic << ": " << payload << "\n";
      });
  std::cout << "  Subscribed to " << order_ids.size() << " order topics\n";

  // 2. Dispatcher: aggregate all events into a single audit log topic
  std::cout << "\n=== Dispatcher: all events → audit.log ===\n";
  EventDispatcher dispatcher(bus, "audit.log");
  for (const auto& topic : all_topics) {
    dispatcher.watch(topic);
  }

  (void)bus.subscribe("audit.log",
                      [](const std::string&, const std::string& payload) {
                        std::cout << "  [audit] " << payload << "\n";
                      });

  // Fire some events
  std::cout << "\n=== Publishing events ===\n";
  bus.publish("order.created", "order #1001");
  bus.publish("payment.charged", "$49.99 for order #1001");
  bus.publish("order.shipped", "order #1001 via FedEx");
  bus.publish("user.login", "alice@example.com");

  // Unsubscribe order handlers
  std::cout << "\n=== Unsubscribe order.* handlers ===\n";
  for (auto id : order_ids) bus.unsubscribe(id);
  bus.publish("order.cancelled", "order #1002");
  std::cout << "  (order handler silent, but audit still active)\n";

  // Cleanup
  dispatcher.unwatch_all();

  std::cout << "\n=== Shutdown ===\n";
  manager.shutdown();

  return 0;
}
