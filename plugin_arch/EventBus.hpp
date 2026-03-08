// Publish-subscribe event bus for plugin-to-plugin broadcast messaging.
//
// Complements ServiceLocator (direct request-response) with a broadcast
// pattern. Plugins publish events by topic string; all subscribers receive them.
//
// Three APIs:
//   String-based:  subscribe(topic, handler) / publish(topic, payload)
//   Type-safe:     subscribe_typed<T>(topic, handler) / publish_typed<T>(topic, event)
//   Vetoable:      subscribe_vetoable(topic, handler) / publish_vetoable(topic, payload)
//
// All subscribe methods accept an optional priority parameter (default 0).
// Higher priority = dispatched first. Same priority = insertion order.
//
// The bus is synchronous: publish() calls all handlers before returning.
// A throwing handler does not prevent other handlers from receiving the event.

#pragma once

#include <algorithm>
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

  // Veto handler: returns true to allow, false to veto.
  using VetoHandler = std::function<bool(const std::string& topic,
                                         const std::string& payload)>;

  // Sentinel for "not yet subscribed". Valid IDs start at 1.
  static constexpr SubscriptionId invalid_id = 0;

  // --- String-based API ---

  // Subscribe to a topic. Higher priority = dispatched first.
  // Throws std::invalid_argument if handler is empty.
  [[nodiscard]] SubscriptionId subscribe(const std::string& topic,
                                         Handler handler,
                                         int priority = 0) {
    if (!handler) {
      throw std::invalid_argument(
          "EventBus::subscribe: handler must not be null");
    }
    SubscriptionId id = next_id_++;
    auto& subs = subscriptions_[topic];
    subs.push_back({id, std::move(handler), priority});
    sort_by_priority(subs);
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

  template <typename T>
  using TypedHandler =
      std::function<void(const std::string& topic, const T& event)>;

  // Subscribe to typed events on a topic. Higher priority = dispatched first.
  // Throws std::invalid_argument if handler is empty.
  template <typename T>
  [[nodiscard]] SubscriptionId subscribe_typed(const std::string& topic,
                                               TypedHandler<T> handler,
                                               int priority = 0) {
    if (!handler) {
      throw std::invalid_argument(
          "EventBus::subscribe_typed: handler must not be null");
    }
    SubscriptionId id = next_id_++;
    auto wrapper = [h = std::move(handler)](const std::string& t,
                                            const std::any& payload) {
      h(t, std::any_cast<const T&>(payload));
    };
    auto& subs = typed_subscriptions_[topic];
    subs.push_back({id, std::move(wrapper), priority});
    sort_by_priority(subs);
    id_to_topic_[id] = topic;
    return id;
  }

  // Publish a typed event. Dispatches to typed subscribers only.
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

  // --- Vetoable API (B5) ---

  // Subscribe a veto handler. Returns true to allow, false to veto.
  // Higher priority = checked first.
  [[nodiscard]] SubscriptionId subscribe_vetoable(const std::string& topic,
                                                  VetoHandler handler,
                                                  int priority = 0) {
    if (!handler) {
      throw std::invalid_argument(
          "EventBus::subscribe_vetoable: handler must not be null");
    }
    SubscriptionId id = next_id_++;
    auto& subs = vetoable_subscriptions_[topic];
    subs.push_back({id, std::move(handler), priority});
    sort_by_priority(subs);
    id_to_topic_[id] = topic;
    return id;
  }

  // Publish with veto support. Veto handlers are called in priority order.
  // If any handler returns false (veto) and stop_on_veto is true, remaining
  // handlers are skipped. Returns true if the event was NOT vetoed.
  [[nodiscard]] bool publish_vetoable(const std::string& topic,
                                      const std::string& payload = {},
                                      bool stop_on_veto = true) {
    auto it = vetoable_subscriptions_.find(topic);
    if (it == vetoable_subscriptions_.end()) return true;
    auto snapshot = it->second;
    bool vetoed = false;
    for (const auto& sub : snapshot) {
      try {
        if (!sub.handler(topic, payload)) {
          vetoed = true;
          if (stop_on_veto) break;
        }
      } catch (...) {
      }
    }
    return !vetoed;
  }

  // --- Common ---

  // Unsubscribe by ID. Works for string, typed, and vetoable subscriptions.
  bool unsubscribe(SubscriptionId id) {
    auto topic_it = id_to_topic_.find(id);
    if (topic_it == id_to_topic_.end()) return false;

    const auto& topic = topic_it->second;

    if (erase_by_id(subscriptions_, topic, id) ||
        erase_typed_by_id(typed_subscriptions_, topic, id) ||
        erase_vetoable_by_id(vetoable_subscriptions_, topic, id)) {
      id_to_topic_.erase(topic_it);
      return true;
    }

    id_to_topic_.erase(topic_it);
    return false;
  }

  void clear() {
    subscriptions_.clear();
    typed_subscriptions_.clear();
    vetoable_subscriptions_.clear();
    id_to_topic_.clear();
  }

  [[nodiscard]] std::size_t subscriber_count(const std::string& topic) const {
    std::size_t count = 0;
    if (auto it = subscriptions_.find(topic); it != subscriptions_.end())
      count += it->second.size();
    if (auto it = typed_subscriptions_.find(topic);
        it != typed_subscriptions_.end())
      count += it->second.size();
    if (auto it = vetoable_subscriptions_.find(topic);
        it != vetoable_subscriptions_.end())
      count += it->second.size();
    return count;
  }

  [[nodiscard]] std::size_t typed_subscriber_count(
      const std::string& topic) const {
    auto it = typed_subscriptions_.find(topic);
    return it != typed_subscriptions_.end() ? it->second.size() : 0;
  }

  [[nodiscard]] std::size_t vetoable_subscriber_count(
      const std::string& topic) const {
    auto it = vetoable_subscriptions_.find(topic);
    return it != vetoable_subscriptions_.end() ? it->second.size() : 0;
  }

  // The next ID that will be assigned. Useful for tracking subscription
  // ranges (e.g. PluginManager enable/disable).
  [[nodiscard]] SubscriptionId next_subscription_id() const {
    return next_id_;
  }

 private:
  struct Subscription {
    SubscriptionId id;
    Handler handler;
    int priority = 0;
  };

  using TypedHandlerWrapper =
      std::function<void(const std::string&, const std::any&)>;

  struct TypedSubscription {
    SubscriptionId id;
    TypedHandlerWrapper handler;
    int priority = 0;
  };

  struct VetoableSubscription {
    SubscriptionId id;
    VetoHandler handler;
    int priority = 0;
  };

  SubscriptionId next_id_ = 1;
  std::unordered_map<std::string, std::vector<Subscription>> subscriptions_;
  std::unordered_map<std::string, std::vector<TypedSubscription>>
      typed_subscriptions_;
  std::unordered_map<std::string, std::vector<VetoableSubscription>>
      vetoable_subscriptions_;
  std::unordered_map<SubscriptionId, std::string> id_to_topic_;

  // Sort by descending priority (stable to preserve insertion order within
  // same priority).
  template <typename Vec>
  static void sort_by_priority(Vec& subs) {
    std::stable_sort(subs.begin(), subs.end(),
                     [](const auto& a, const auto& b) {
                       return a.priority > b.priority;
                     });
  }

  // Erase helpers for unsubscribe — search each map type.
  static bool erase_by_id(
      std::unordered_map<std::string, std::vector<Subscription>>& map,
      const std::string& topic, SubscriptionId id) {
    auto it = map.find(topic);
    if (it == map.end()) return false;
    auto& subs = it->second;
    for (auto sit = subs.begin(); sit != subs.end(); ++sit) {
      if (sit->id == id) { subs.erase(sit); return true; }
    }
    return false;
  }

  static bool erase_typed_by_id(
      std::unordered_map<std::string, std::vector<TypedSubscription>>& map,
      const std::string& topic, SubscriptionId id) {
    auto it = map.find(topic);
    if (it == map.end()) return false;
    auto& subs = it->second;
    for (auto sit = subs.begin(); sit != subs.end(); ++sit) {
      if (sit->id == id) { subs.erase(sit); return true; }
    }
    return false;
  }

  static bool erase_vetoable_by_id(
      std::unordered_map<std::string, std::vector<VetoableSubscription>>& map,
      const std::string& topic, SubscriptionId id) {
    auto it = map.find(topic);
    if (it == map.end()) return false;
    auto& subs = it->second;
    for (auto sit = subs.begin(); sit != subs.end(); ++sit) {
      if (sit->id == id) { subs.erase(sit); return true; }
    }
    return false;
  }
};

}  // namespace plugin_arch
