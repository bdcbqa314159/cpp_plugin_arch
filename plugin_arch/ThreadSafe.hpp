// Thread-safe wrappers using std::shared_mutex (readers-writer lock).
//
// Zero cost for single-threaded hosts — they use the originals unchanged.
// Multi-threaded hosts wrap the component they need:
//
//   ThreadSafe<ServiceLocator> locator;
//   ThreadSafe<PluginRegistry> registry;
//   ThreadSafe<HotPluginLoader<T>> hot_loader;
//   ThreadSafe<EventBus> bus;
//   ThreadSafe<PluginManager> manager;
//
// Read operations take a shared lock, write operations take a unique lock.

#pragma once

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

#include "EventBus.hpp"
#include "HotPluginLoader.hpp"
#include "PluginManager.hpp"
#include "PluginRegistry.hpp"
#include "ServiceLocator.hpp"

namespace plugin_arch {

// Primary template — not defined. Only the explicit specializations below
// are usable. Attempting ThreadSafe<SomeOtherType> gives a compile error.
template <typename T>
class ThreadSafe;

// --- ThreadSafe<ServiceLocator> ---

template <>
class ThreadSafe<ServiceLocator> {
 public:
  void add(const std::shared_ptr<IPlugin>& service) {
    std::unique_lock lock(mutex_);
    inner_.add(service);
  }

  template <typename T>
  [[nodiscard]] std::shared_ptr<T> get(const std::string& type) const {
    std::shared_lock lock(mutex_);
    return inner_.get<T>(type);
  }

  template <typename T>
  [[nodiscard]] std::vector<std::shared_ptr<T>> get_all(const std::string& type) const {
    std::shared_lock lock(mutex_);
    return inner_.get_all<T>(type);
  }

  void cleanup() {
    std::unique_lock lock(mutex_);
    inner_.cleanup();
  }

  void clear() {
    std::unique_lock lock(mutex_);
    inner_.clear();
  }

  [[nodiscard]] std::size_t size() const {
    std::shared_lock lock(mutex_);
    return inner_.size();
  }

 private:
  ServiceLocator inner_;
  mutable std::shared_mutex mutex_;
};

// --- ThreadSafe<PluginRegistry> ---

template <>
class ThreadSafe<PluginRegistry> {
 public:
  [[nodiscard]] std::size_t scan(const std::filesystem::path& directory) {
    std::unique_lock lock(mutex_);
    return inner_.scan(directory);
  }

  [[nodiscard]] std::vector<PluginEntry> get_all(const std::string& type) const {
    std::shared_lock lock(mutex_);
    return inner_.get_all(type);
  }

  [[nodiscard]] std::optional<PluginEntry> get(const std::string& name) const {
    std::shared_lock lock(mutex_);
    return inner_.get(name);
  }

  [[nodiscard]] std::vector<PluginEntry> entries() const {
    std::shared_lock lock(mutex_);
    return std::vector<PluginEntry>(inner_.entries().begin(), inner_.entries().end());
  }

  [[nodiscard]] std::vector<ErrorRecord> errors() const {
    std::shared_lock lock(mutex_);
    return std::vector<ErrorRecord>(inner_.errors().begin(), inner_.errors().end());
  }

  void clear() {
    std::unique_lock lock(mutex_);
    inner_.clear();
  }

 private:
  PluginRegistry inner_;
  mutable std::shared_mutex mutex_;
};

// --- ThreadSafe<HotPluginLoader<T>> ---

template <typename T>
class ThreadSafe<HotPluginLoader<T>> {
 public:
  explicit ThreadSafe(std::string library_path,
                      std::string alloc_symbol = "allocator",
                      std::string dealloc_symbol = "deallocator")
      : inner_(std::move(library_path), std::move(alloc_symbol),
               std::move(dealloc_symbol)) {}

  [[nodiscard]] std::shared_ptr<T> get_instance() const {
    std::shared_lock lock(mutex_);
    return inner_.get_instance();
  }

  [[nodiscard]] bool check_and_reload() {
    std::unique_lock lock(mutex_);
    return inner_.check_and_reload();
  }

  void reload() {
    std::unique_lock lock(mutex_);
    inner_.reload();
  }

  [[nodiscard]] std::string library_path() const {
    std::shared_lock lock(mutex_);
    return inner_.library_path();
  }

 private:
  HotPluginLoader<T> inner_;
  mutable std::shared_mutex mutex_;
};

// --- ThreadSafe<EventBus> ---
//
// Uses std::recursive_mutex (not shared_mutex) so that handlers invoked
// during publish() can safely call subscribe()/unsubscribe() on the same
// instance without deadlocking. EventBus::publish() already takes a
// snapshot of handlers before iterating, so the re-entrant mutation is safe.

template <>
class ThreadSafe<EventBus> {
 public:
  [[nodiscard]] EventBus::SubscriptionId subscribe(const std::string& topic,
                                                    EventBus::Handler handler,
                                                    int priority = 0) {
    std::unique_lock lock(mutex_);
    return inner_.subscribe(topic, std::move(handler), priority);
  }

  void publish(const std::string& topic, const std::string& payload = {}) {
    std::unique_lock lock(mutex_);
    inner_.publish(topic, payload);
  }

  template <typename T>
  [[nodiscard]] EventBus::SubscriptionId subscribe_typed(
      const std::string& topic, EventBus::TypedHandler<T> handler,
      int priority = 0) {
    std::unique_lock lock(mutex_);
    return inner_.subscribe_typed<T>(topic, std::move(handler), priority);
  }

  template <typename T>
  void publish_typed(const std::string& topic, const T& event) {
    std::unique_lock lock(mutex_);
    inner_.publish_typed<T>(topic, event);
  }

  [[nodiscard]] EventBus::SubscriptionId subscribe_vetoable(
      const std::string& topic, EventBus::VetoHandler handler,
      int priority = 0) {
    std::unique_lock lock(mutex_);
    return inner_.subscribe_vetoable(topic, std::move(handler), priority);
  }

  [[nodiscard]] bool publish_vetoable(const std::string& topic,
                                       const std::string& payload = {},
                                       bool stop_on_veto = true) {
    std::unique_lock lock(mutex_);
    return inner_.publish_vetoable(topic, payload, stop_on_veto);
  }

  bool unsubscribe(EventBus::SubscriptionId id) {
    std::unique_lock lock(mutex_);
    return inner_.unsubscribe(id);
  }

  void clear() {
    std::unique_lock lock(mutex_);
    inner_.clear();
  }

  [[nodiscard]] std::size_t subscriber_count(const std::string& topic) const {
    std::unique_lock lock(mutex_);
    return inner_.subscriber_count(topic);
  }

  [[nodiscard]] std::size_t typed_subscriber_count(
      const std::string& topic) const {
    std::unique_lock lock(mutex_);
    return inner_.typed_subscriber_count(topic);
  }

  [[nodiscard]] std::size_t vetoable_subscriber_count(
      const std::string& topic) const {
    std::unique_lock lock(mutex_);
    return inner_.vetoable_subscriber_count(topic);
  }

  [[nodiscard]] EventBus::SubscriptionId next_subscription_id() const {
    std::unique_lock lock(mutex_);
    return inner_.next_subscription_id();
  }

 private:
  EventBus inner_;
  mutable std::recursive_mutex mutex_;
};

// --- ThreadSafe<PluginManager> ---

template <>
class ThreadSafe<PluginManager> {
 public:
  void load_all(const std::filesystem::path& directory,
                const PluginManager::ConfigMap& config_map = {},
                LoadPolicy policy = LoadPolicy::strict) {
    std::unique_lock lock(mutex_);
    inner_.load_all(directory, config_map, policy);
  }

  void load_all(PluginRegistry& registry,
                const PluginManager::ConfigMap& config_map = {},
                LoadPolicy policy = LoadPolicy::strict) {
    std::unique_lock lock(mutex_);
    inner_.load_all(registry, config_map, policy);
  }

  void load_all_parallel(const std::filesystem::path& directory,
                         const PluginManager::ConfigMap& config_map = {},
                         LoadPolicy policy = LoadPolicy::strict) {
    std::unique_lock lock(mutex_);
    inner_.load_all_parallel(directory, config_map, policy);
  }

  void add_mixin_wirer(PluginManager::MixinWirer wirer) {
    std::unique_lock lock(mutex_);
    inner_.add_mixin_wirer(std::move(wirer));
  }

  void add_plugin(std::shared_ptr<IPlugin> instance,
                  const PluginEntry& entry,
                  const PluginManager::ConfigMap& config_map = {}) {
    std::unique_lock lock(mutex_);
    inner_.add_plugin(std::move(instance), entry, config_map);
  }

  void unload(const std::string& name) {
    std::unique_lock lock(mutex_);
    inner_.unload(name);
  }

  void reload(const std::string& name,
              const PluginManager::ConfigMap& config_map = {}) {
    std::unique_lock lock(mutex_);
    inner_.reload(name, config_map);
  }

  void disable(const std::string& name) {
    std::unique_lock lock(mutex_);
    inner_.disable(name);
  }

  void enable(const std::string& name,
              const PluginManager::ConfigMap& config_map = {}) {
    std::unique_lock lock(mutex_);
    inner_.enable(name, config_map);
  }

  void shutdown() {
    std::unique_lock lock(mutex_);
    inner_.shutdown();
  }

  [[nodiscard]] bool is_loaded(const std::string& name) const {
    std::shared_lock lock(mutex_);
    return inner_.is_loaded(name);
  }

  [[nodiscard]] bool is_enabled(const std::string& name) const {
    std::shared_lock lock(mutex_);
    return inner_.is_enabled(name);
  }

  [[nodiscard]] std::shared_ptr<IPlugin> get_plugin(
      const std::string& name) const {
    std::shared_lock lock(mutex_);
    return inner_.get_plugin(name);
  }

  template <typename T>
  [[nodiscard]] std::shared_ptr<T> get_service(
      const std::string& type) const {
    std::shared_lock lock(mutex_);
    return inner_.get_service<T>(type);
  }

  [[nodiscard]] std::vector<PluginManager::PluginHealthReport> check_health(
      bool include_healthy = false) const {
    std::shared_lock lock(mutex_);
    return inner_.check_health(include_healthy);
  }

  [[nodiscard]] std::vector<std::string> dependents_of(
      const std::string& name) const {
    std::shared_lock lock(mutex_);
    return inner_.dependents_of(name);
  }

  [[nodiscard]] std::vector<ErrorRecord> load_errors() const {
    std::shared_lock lock(mutex_);
    return std::vector<ErrorRecord>(inner_.load_errors().begin(),
                                    inner_.load_errors().end());
  }

  // --- C1: Plugin groups ---

  [[nodiscard]] std::vector<std::string> plugins_with_capability(
      const std::string& capability) const {
    std::shared_lock lock(mutex_);
    return inner_.plugins_with_capability(capability);
  }

  void disable_group(const std::string& capability) {
    std::unique_lock lock(mutex_);
    inner_.disable_group(capability);
  }

  void enable_group(const std::string& capability,
                    const PluginManager::ConfigMap& config_map = {}) {
    std::unique_lock lock(mutex_);
    inner_.enable_group(capability, config_map);
  }

  void unload_group(const std::string& capability) {
    std::unique_lock lock(mutex_);
    inner_.unload_group(capability);
  }

  // --- C3: Dependency graph ---

  [[nodiscard]] std::vector<std::string> dependency_graph() const {
    std::shared_lock lock(mutex_);
    return inner_.dependency_graph();
  }

  // --- C4: Observers ---

  void add_observer(PluginObserver* observer) {
    std::unique_lock lock(mutex_);
    inner_.add_observer(observer);
  }

  void remove_observer(PluginObserver* observer) {
    std::unique_lock lock(mutex_);
    inner_.remove_observer(observer);
  }

  // --- Additional accessors ---

  [[nodiscard]] std::vector<PluginManager::LoadedPlugin> loaded_plugins() const {
    std::shared_lock lock(mutex_);
    return std::vector<PluginManager::LoadedPlugin>(
        inner_.loaded_plugins().begin(), inner_.loaded_plugins().end());
  }

 private:
  PluginManager inner_;
  mutable std::shared_mutex mutex_;
};

}  // namespace plugin_arch
