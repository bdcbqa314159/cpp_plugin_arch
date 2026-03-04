// Publish-subscribe event bus for plugin-to-plugin broadcast messaging.
//
// Complements ServiceLocator (direct request-response) with a broadcast
// pattern. Plugins publish events by topic string; all subscribers receive them.
//
// Events are std::string payloads — plugins choose their own serialization.
// The bus is synchronous: publish() calls all handlers before returning.
// A throwing handler does not prevent other handlers from receiving the event.

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

  // Sentinel for "not yet subscribed". Valid IDs start at 1.
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
    id_to_topic_[id] = topic;
    return id;
  }

  // Unsubscribe by ID. Returns true if the subscription was found and removed.
  bool unsubscribe(SubscriptionId id) {
    auto topic_it = id_to_topic_.find(id);
    if (topic_it == id_to_topic_.end()) return false;

    auto& subs = subscriptions_[topic_it->second];
    for (auto it = subs.begin(); it != subs.end(); ++it) {
      if (it->id == id) {
        subs.erase(it);
        break;
      }
    }
    id_to_topic_.erase(topic_it);
    return true;
  }

  // Publish an event to all subscribers of the given topic.
  // Safe to call subscribe/unsubscribe from within handlers.
  // A throwing handler does not prevent remaining handlers from firing.
  void publish(const std::string& topic, const std::string& payload = {}) {
    auto it = subscriptions_.find(topic);
    if (it == subscriptions_.end()) return;
    // Snapshot: handlers may subscribe/unsubscribe during iteration.
    auto snapshot = it->second;
    for (const auto& sub : snapshot) {
      try {
        sub.handler(topic, payload);
      } catch (...) {
        // Swallow — one bad handler must not block others.
      }
    }
  }

  void clear() {
    subscriptions_.clear();
    id_to_topic_.clear();
  }

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
  std::unordered_map<SubscriptionId, std::string> id_to_topic_;
};

}  // namespace plugin_arch
