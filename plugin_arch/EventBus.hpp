// Publish-subscribe event bus for plugin-to-plugin broadcast messaging.
//
// Complements ServiceLocator (direct request-response) with a broadcast
// pattern. Plugins publish events by topic string; all subscribers receive them.
//
// Two APIs:
//   String-based:  subscribe(topic, handler) / publish(topic, payload)
//   Type-safe:     subscribe_typed<T>(topic, handler) / publish_typed<T>(topic, event)
//
// The bus is synchronous: publish() calls all handlers before returning.
// A throwing handler does not prevent other handlers from receiving the event.

#pragma once

#include <any>
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

  // --- String-based API (original) ---

  // Subscribe to a topic. Returns an ID for unsubscribing.
  // Throws std::invalid_argument if handler is empty.
  [[nodiscard]] SubscriptionId subscribe(const std::string& topic,
                                         Handler handler) {
    if (!handler) {
      throw std::invalid_argument(
          "EventBus::subscribe: handler must not be null");
    }
    SubscriptionId id = next_id_++;
    subscriptions_[topic].push_back({id, std::move(handler)});
    id_to_topic_[id] = topic;
    return id;
  }

  // Publish an event to all subscribers of the given topic.
  // Safe to call subscribe/unsubscribe from within handlers.
  // A throwing handler does not prevent remaining handlers from firing.
  void publish(const std::string& topic, const std::string& payload = {}) {
    auto it = subscriptions_.find(topic);
    if (it == subscriptions_.end()) return;
    auto snapshot = it->second;
    for (const auto& sub : snapshot) {
      try {
        sub.handler(topic, payload);
      } catch (...) {
      }
    }
  }

  // --- Typed API ---

  // Type-safe handler: receives the topic and a const reference to the event.
  template <typename T>
  using TypedHandler =
      std::function<void(const std::string& topic, const T& event)>;

  // Subscribe to typed events on a topic. Returns an ID for unsubscribing.
  // Only receives events published via publish_typed<T>() with matching T.
  // Throws std::invalid_argument if handler is empty.
  template <typename T>
  [[nodiscard]] SubscriptionId subscribe_typed(const std::string& topic,
                                               TypedHandler<T> handler) {
    if (!handler) {
      throw std::invalid_argument(
          "EventBus::subscribe_typed: handler must not be null");
    }
    SubscriptionId id = next_id_++;
    auto wrapper = [h = std::move(handler)](const std::string& t,
                                            const std::any& payload) {
      h(t, std::any_cast<const T&>(payload));
    };
    typed_subscriptions_[topic].push_back({id, std::move(wrapper)});
    id_to_topic_[id] = topic;
    return id;
  }

  // Publish a typed event. Dispatches to typed subscribers only.
  // String-based subscribers on the same topic are NOT notified.
  template <typename T>
  void publish_typed(const std::string& topic, const T& event) {
    auto it = typed_subscriptions_.find(topic);
    if (it == typed_subscriptions_.end()) return;
    auto snapshot = it->second;
    std::any wrapped = event;
    for (const auto& sub : snapshot) {
      try {
        sub.handler(topic, wrapped);
      } catch (...) {
      }
    }
  }

  // --- Common ---

  // Unsubscribe by ID. Works for both string and typed subscriptions.
  // Returns true if the subscription was found and removed.
  bool unsubscribe(SubscriptionId id) {
    auto topic_it = id_to_topic_.find(id);
    if (topic_it == id_to_topic_.end()) return false;

    const auto& topic = topic_it->second;

    // Try string subscriptions
    if (auto it = subscriptions_.find(topic); it != subscriptions_.end()) {
      auto& subs = it->second;
      for (auto sit = subs.begin(); sit != subs.end(); ++sit) {
        if (sit->id == id) {
          subs.erase(sit);
          id_to_topic_.erase(topic_it);
          return true;
        }
      }
    }

    // Try typed subscriptions
    if (auto it = typed_subscriptions_.find(topic);
        it != typed_subscriptions_.end()) {
      auto& subs = it->second;
      for (auto sit = subs.begin(); sit != subs.end(); ++sit) {
        if (sit->id == id) {
          subs.erase(sit);
          id_to_topic_.erase(topic_it);
          return true;
        }
      }
    }

    id_to_topic_.erase(topic_it);
    return false;
  }

  void clear() {
    subscriptions_.clear();
    typed_subscriptions_.clear();
    id_to_topic_.clear();
  }

  [[nodiscard]] std::size_t subscriber_count(const std::string& topic) const {
    std::size_t count = 0;
    if (auto it = subscriptions_.find(topic); it != subscriptions_.end()) {
      count += it->second.size();
    }
    if (auto it = typed_subscriptions_.find(topic);
        it != typed_subscriptions_.end()) {
      count += it->second.size();
    }
    return count;
  }

  // Count only typed subscribers for a topic.
  [[nodiscard]] std::size_t typed_subscriber_count(
      const std::string& topic) const {
    auto it = typed_subscriptions_.find(topic);
    return it != typed_subscriptions_.end() ? it->second.size() : 0;
  }

 private:
  struct Subscription {
    SubscriptionId id;
    Handler handler;
  };

  using TypedHandlerWrapper =
      std::function<void(const std::string&, const std::any&)>;

  struct TypedSubscription {
    SubscriptionId id;
    TypedHandlerWrapper handler;
  };

  SubscriptionId next_id_ = 1;  // 0 is reserved as invalid_id
  std::unordered_map<std::string, std::vector<Subscription>> subscriptions_;
  std::unordered_map<std::string, std::vector<TypedSubscription>>
      typed_subscriptions_;
  std::unordered_map<SubscriptionId, std::string> id_to_topic_;
};

}  // namespace plugin_arch
