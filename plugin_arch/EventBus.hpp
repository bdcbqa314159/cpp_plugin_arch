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
#include <string>
#include <unordered_map>
#include <vector>

namespace plugin_arch {

class EventBus {
 public:
  using Handler = std::function<void(const std::string& topic,
                                     const std::string& payload)>;
  using SubscriptionId = std::size_t;

  // Subscribe to a topic. Returns an ID for unsubscribing.
  [[nodiscard]] SubscriptionId subscribe(const std::string& topic,
                                         Handler handler) {
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
  void publish(const std::string& topic, const std::string& payload = {}) {
    auto it = subscriptions_.find(topic);
    if (it == subscriptions_.end()) return;
    for (const auto& sub : it->second) {
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

  SubscriptionId next_id_ = 0;
  std::unordered_map<std::string, std::vector<Subscription>> subscriptions_;
};

}  // namespace plugin_arch
