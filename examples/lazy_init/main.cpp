// Lazy initialization — shows how a host defers plugin loading until
// a service is first requested.
//
// Pattern: wrap ServiceLocator::get<T>() with a fallback that calls
// add_plugin() on a cache miss, then retries the lookup.

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

#include "ILifecycleAware.hpp"
#include "IServiceAware.hpp"
#include "PluginManager.hpp"
#include "ServiceLocator.hpp"

using namespace plugin_arch;

// --- Demo plugins ---

class DatabasePlugin : public IPlugin, public ILifecycleAware {
 public:
  const std::string& name() const override {
    static const std::string n = "DatabasePlugin";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "database";
    return t;
  }
  void on_init() override { std::cout << "  [Database] Initialized\n"; }
  void on_shutdown() override { std::cout << "  [Database] Shut down\n"; }

  std::string query(const std::string& sql) const {
    return "result of: " + sql;
  }
};

class CachePlugin : public IPlugin, public ILifecycleAware {
 public:
  const std::string& name() const override {
    static const std::string n = "CachePlugin";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "cache";
    return t;
  }
  void on_init() override { std::cout << "  [Cache] Initialized\n"; }
  void on_shutdown() override { std::cout << "  [Cache] Shut down\n"; }

  std::string get(const std::string& key) const {
    return "cached:" + key;
  }
};

// --- Factory registry (host-side code) ---

using PluginFactory = std::function<std::shared_ptr<IPlugin>()>;

static PluginEntry make_entry(const std::string& name,
                              const std::string& type) {
  PluginEntry e;
  e.name = name;
  e.type = type;
  e.version = "1.0.0";
  return e;
}

// --- Lazy service locator wrapper (host-side code) ---

class LazyServiceLocator {
 public:
  LazyServiceLocator(PluginManager& manager, ServiceLocator& locator)
      : manager_(manager), locator_(locator) {}

  // Register a factory for lazy creation.
  void register_factory(const std::string& type, const std::string& name,
                        PluginFactory factory) {
    factories_[type] = {name, std::move(factory)};
  }

  // Get a service — loads the plugin on first access.
  template <typename T>
  std::shared_ptr<T> get(const std::string& type) {
    // Try existing service first
    auto svc = locator_.get<T>(type);
    if (svc) return svc;

    // Lazy load: create from factory, register, retry
    auto it = factories_.find(type);
    if (it == factories_.end()) {
      std::cout << "  [lazy] No factory for type '" << type << "'\n";
      return nullptr;
    }

    std::cout << "  [lazy] First request for '" << type
              << "' — loading on demand\n";
    auto instance = it->second.factory();
    manager_.add_plugin(instance, make_entry(it->second.name, type));
    locator_.add(instance);
    factories_.erase(it);  // one-shot: factory consumed

    return locator_.get<T>(type);
  }

 private:
  struct FactoryEntry {
    std::string name;
    PluginFactory factory;
  };

  PluginManager& manager_;
  ServiceLocator& locator_;
  std::unordered_map<std::string, FactoryEntry> factories_;
};

// --- Main ---

int main() {
  PluginManager manager;
  ServiceLocator locator;
  LazyServiceLocator lazy(manager, locator);

  // Register factories — nothing loaded yet
  std::cout << "=== Registering factories (no plugins loaded yet) ===\n";
  lazy.register_factory("database", "DatabasePlugin",
                        [] { return std::make_shared<DatabasePlugin>(); });
  lazy.register_factory("cache", "CachePlugin",
                        [] { return std::make_shared<CachePlugin>(); });

  std::cout << "  Loaded plugins: " << manager.loaded_plugins().size() << "\n";

  // First request triggers lazy load
  std::cout << "\n=== First database request ===\n";
  auto db = lazy.get<DatabasePlugin>("database");
  if (db) {
    std::cout << "  " << db->query("SELECT * FROM users") << "\n";
  }

  // Second request returns cached instance
  std::cout << "\n=== Second database request (already loaded) ===\n";
  auto db2 = lazy.get<DatabasePlugin>("database");
  if (db2) {
    std::cout << "  Same instance? " << (db.get() == db2.get() ? "yes" : "no")
              << "\n";
  }

  // Cache loaded on demand too
  std::cout << "\n=== First cache request ===\n";
  auto cache = lazy.get<CachePlugin>("cache");
  if (cache) {
    std::cout << "  " << cache->get("user:42") << "\n";
  }

  // Unknown type
  std::cout << "\n=== Request for unregistered type ===\n";
  auto unknown = lazy.get<IPlugin>("messaging");

  std::cout << "\n  Loaded plugins: " << manager.loaded_plugins().size()
            << "\n";

  std::cout << "\n=== Shutdown ===\n";
  manager.shutdown();

  return 0;
}
