// Event replay / history — shows how an observer records lifecycle
// events in a ring buffer and replays them on demand.
//
// Pattern: implement PluginObserver with a fixed-size circular buffer,
// record timestamped events, replay for debugging.

#include <chrono>
#include <deque>
#include <iostream>
#include <string>

#include "EventBus.hpp"
#include "ILifecycleAware.hpp"
#include "PluginManager.hpp"
#include "PluginObserver.hpp"

using namespace plugin_arch;

// --- Event record ---

struct EventRecord {
  std::chrono::steady_clock::time_point timestamp;
  std::string category;  // "lifecycle" or "bus"
  std::string action;    // "loaded", "unloaded", "published", etc.
  std::string detail;    // plugin name or topic:payload
};

// --- Ring buffer recorder (host-side code) ---

class EventRecorder : public PluginObserver {
 public:
  explicit EventRecorder(std::size_t max_events = 100)
      : max_events_(max_events) {}

  // PluginObserver overrides
  void on_plugin_loaded(const std::string& name,
                        const std::string&,
                        const PluginEntry&) override {
    record("lifecycle", "loaded", name);
  }
  void on_plugin_unloaded(const std::string& name,
                          const std::string&,
                          const PluginEntry&) override {
    record("lifecycle", "unloaded", name);
  }
  void on_plugin_enabled(const std::string& name,
                         const std::string&,
                         const PluginEntry&) override {
    record("lifecycle", "enabled", name);
  }
  void on_plugin_disabled(const std::string& name,
                          const std::string&,
                          const PluginEntry&) override {
    record("lifecycle", "disabled", name);
  }
  void on_plugin_reloaded(const std::string& name,
                          const std::string&,
                          const PluginEntry&) override {
    record("lifecycle", "reloaded", name);
  }

  // Record a bus event (called manually by the host's bus tap)
  void record_bus_event(const std::string& topic,
                        const std::string& payload) {
    record("bus", "published", topic + ": " + payload);
  }

  // Replay all recorded events
  void replay(std::ostream& out) const {
    if (events_.empty()) {
      out << "  (no events recorded)\n";
      return;
    }
    auto base = events_.front().timestamp;
    for (const auto& evt : events_) {
      auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
          evt.timestamp - base);
      out << "  [+" << ms.count() << "ms] [" << evt.category << "] "
          << evt.action << ": " << evt.detail << "\n";
    }
  }

  std::size_t size() const { return events_.size(); }
  void clear() { events_.clear(); }

 private:
  void record(const std::string& category, const std::string& action,
              const std::string& detail) {
    events_.push_back(
        {std::chrono::steady_clock::now(), category, action, detail});
    while (events_.size() > max_events_) {
      events_.pop_front();
    }
  }

  std::size_t max_events_;
  std::deque<EventRecord> events_;
};

// --- Demo plugins ---

class AlphaPlugin : public IPlugin, public ILifecycleAware {
 public:
  const std::string& name() const override {
    static const std::string n = "Alpha";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "alpha";
    return t;
  }
  void on_init() override {}
  void on_shutdown() override {}
};

class BetaPlugin : public IPlugin, public ILifecycleAware {
 public:
  const std::string& name() const override {
    static const std::string n = "Beta";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "beta";
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
  EventRecorder recorder(50);  // keep last 50 events — declared first so it outlives manager
  PluginManager manager;
  EventBus bus;

  manager.add_observer(&recorder);

  // Tap into the bus to record published events
  (void)bus.subscribe("order.created",
                      [&recorder](const std::string& topic,
                                  const std::string& payload) {
                        recorder.record_bus_event(topic, payload);
                      });
  (void)bus.subscribe("order.shipped",
                      [&recorder](const std::string& topic,
                                  const std::string& payload) {
                        recorder.record_bus_event(topic, payload);
                      });

  // Generate some activity
  std::cout << "=== Generating activity ===\n";
  manager.add_plugin(std::make_shared<AlphaPlugin>(),
                     make_entry("Alpha", "alpha"));
  manager.add_plugin(std::make_shared<BetaPlugin>(),
                     make_entry("Beta", "beta"));

  manager.disable("Alpha");
  bus.publish("order.created", "order #1001");
  manager.enable("Alpha");
  bus.publish("order.shipped", "order #1001 via UPS");
  manager.unload("Beta");

  // Replay the full history
  std::cout << "\n=== Event replay (" << recorder.size() << " events) ===\n";
  recorder.replay(std::cout);

  // Clear and continue
  std::cout << "\n=== Clear history, generate more ===\n";
  recorder.clear();
  manager.add_plugin(std::make_shared<BetaPlugin>(),
                     make_entry("Beta", "beta"));
  bus.publish("order.created", "order #1002");

  std::cout << "\n=== Replay after clear (" << recorder.size()
            << " events) ===\n";
  recorder.replay(std::cout);

  std::cout << "\n=== Shutdown ===\n";
  manager.shutdown();

  return 0;
}
