// Publish-subscribe event bus for plugin-to-plugin broadcast messaging.
//
// Complements ServiceLocator (direct request-response) with a broadcast
// pattern. Plugins publish events by topic string; all subscribers receive them.
//
// Events are std::string payloads — plugins choose their own serialization.
// The bus is synchronous: publish() calls all handlers before returning.

#pragma once

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace plugin_arch {

class EventBus {
 public:
  using Handler = std::function<void(const std::string& topic,
                                     const std::string& payload)>;
  using SubscriptionId = std::size_t;

  // Invalid subscription ID — returned when subscribe() receives a null handler.
  static constexpr SubscriptionId invalid_id = 0;

  // Subscribe to a topic. Returns an ID for unsubscribing.
  // Throws std::invalid_argument if handler is empty.
  [[nodiscard]] SubscriptionId subscribe(const std::string& topic,
                                         Handler handler) {
    if (!handler) {
      throw std::invalid_argument("EventBus::subscribe: handler must not be null");
    }
    SubscriptionId id = next_id_++;
    subscriptions_[topic].push_back({id, std::move(handler)});
    return id;
  }

  // Unsubscribe by ID. Returns true if the subscription was found and removed.
  bool unsubscribe(SubscriptionId id) {
    for (auto& [topic, subs] : subscriptions_) {
      for (auto it = subs.begin(); it != subs.end(); ++it) {
        if (it->id == id) {
          subs.erase(it);
          return true;
        }
      }
    }
    return false;
  }

  // Publish an event to all subscribers of the given topic.
  // Safe to call subscribe/unsubscribe from within handlers.
  void publish(const std::string& topic, const std::string& payload = {}) {
    auto it = subscriptions_.find(topic);
    if (it == subscriptions_.end()) return;
    // Snapshot: handlers may subscribe/unsubscribe during iteration.
    auto snapshot = it->second;
    for (const auto& sub : snapshot) {
      sub.handler(topic, payload);
    }
  }

  void clear() { subscriptions_.clear(); }

  [[nodiscard]] std::size_t subscriber_count(const std::string& topic) const {
    auto it = subscriptions_.find(topic);
    return it != subscriptions_.end() ? it->second.size() : 0;
  }

 private:
  struct Subscription {
    SubscriptionId id;
    Handler handler;
  };

  SubscriptionId next_id_ = 1;  // 0 is reserved as invalid_id
  std::unordered_map<std::string, std::vector<Subscription>> subscriptions_;
};

}  // namespace plugin_arch
