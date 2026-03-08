// Features demo — demonstrates B-tier PluginManager features using add_plugin()
// (no filesystem loading needed).
//
// Features shown:
//   - Health checks (B1): IHealthAware mixin, check_health()
//   - Enable/disable (B2): disable(), enable(), get_service<T>()
//   - Event priority (B3): higher-priority handlers fire first
//   - Conflict detection (B4): IConflictAware prevents co-loading
//   - Vetoable events (B5): handlers can veto event propagation

#include <iostream>
#include <string>
#include <vector>

#include "IConflictAware.hpp"
#include "IEventAware.hpp"
#include "IHealthAware.hpp"
#include "ILifecycleAware.hpp"
#include "PluginManager.hpp"

using namespace plugin_arch;

// --- Demo plugins (in-process, no shared libraries) ---

class DatabasePlugin : public IPlugin,
                       public ILifecycleAware,
                       public IHealthAware,
                       public IEventAware {
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

  void on_init() override {
    std::cout << "  [DB] Initialized (connected)\n";
    connected_ = true;
  }
  void on_shutdown() override {
    std::cout << "  [DB] Shut down (disconnected)\n";
    connected_ = false;
  }

  bool is_healthy() const override { return connected_; }
  HealthStatus health_status() const override {
    if (connected_) return {true, "connected"};
    return {false, "connection lost"};
  }

  void set_event_bus(EventBus& bus) override {
    (void)bus.subscribe(
        "query",
        [this](const std::string&, const std::string& sql) {
          std::cout << "  [DB] Executing: " << sql << "\n";
        },
        /*priority=*/10);  // high priority: DB processes queries first
  }

  void simulate_disconnect() { connected_ = false; }

 private:
  bool connected_ = false;
};

class CachePlugin : public IPlugin,
                    public ILifecycleAware,
                    public IEventAware {
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

  void set_event_bus(EventBus& bus) override {
    (void)bus.subscribe(
        "query",
        [](const std::string&, const std::string& sql) {
          std::cout << "  [Cache] Checking cache for: " << sql << "\n";
        },
        /*priority=*/20);  // higher priority: cache checked BEFORE database
  }
};

class ConflictingCachePlugin : public IPlugin, public IConflictAware {
 public:
  const std::string& name() const override {
    static const std::string n = "ConflictingCache";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "alt_cache";
    return t;
  }
  std::vector<std::string> conflicts() const override { return {"cache"}; }
};

static PluginEntry make_entry(const std::string& name,
                              const std::string& type,
                              const std::string& version = "1.0.0") {
  PluginEntry e;
  e.name = name;
  e.type = type;
  e.version = version;
  return e;
}

int main() {
  PluginManager manager;

  // === 1. Load plugins ===
  std::cout << "=== Loading plugins ===\n";
  auto db = std::make_shared<DatabasePlugin>();
  auto cache = std::make_shared<CachePlugin>();
  manager.add_plugin(db, make_entry("DatabasePlugin", "database"));
  manager.add_plugin(cache, make_entry("CachePlugin", "cache"));

  // === 2. Event priority (B3) ===
  std::cout << "\n=== Event priority: cache (pri=20) fires before DB (pri=10) ===\n";
  manager.event_bus().publish("query", "SELECT * FROM users");

  // === 3. Vetoable events (B5) ===
  std::cout << "\n=== Vetoable event: admin check ===\n";
  (void)manager.event_bus().subscribe_vetoable(
      "admin_action",
      [](const std::string&, const std::string& action) -> bool {
        bool allowed = (action == "view_logs");
        std::cout << "  [Guard] Action '" << action << "' "
                  << (allowed ? "ALLOWED" : "VETOED") << "\n";
        return allowed;
      });
  bool ok1 = manager.event_bus().publish_vetoable("admin_action", "view_logs");
  std::cout << "  Result: " << (ok1 ? "proceeded" : "blocked") << "\n";
  bool ok2 = manager.event_bus().publish_vetoable("admin_action", "delete_all");
  std::cout << "  Result: " << (ok2 ? "proceeded" : "blocked") << "\n";

  // === 4. Health checks (B1) ===
  std::cout << "\n=== Health check (all healthy) ===\n";
  auto reports = manager.check_health(/*include_healthy=*/true);
  for (const auto& r : reports) {
    std::cout << "  " << r.plugin_name << ": "
              << (r.status.healthy ? "healthy" : "UNHEALTHY")
              << " — " << r.status.message << "\n";
  }

  db->simulate_disconnect();
  std::cout << "\n=== Health check (after disconnect) ===\n";
  reports = manager.check_health();
  for (const auto& r : reports) {
    std::cout << "  " << r.plugin_name << ": UNHEALTHY — " << r.status.message
              << "\n";
  }

  // === 5. Enable/disable (B2) ===
  std::cout << "\n=== Disable CachePlugin ===\n";
  manager.disable("CachePlugin");
  std::cout << "  CachePlugin enabled? " << manager.is_enabled("CachePlugin")
            << "\n";
  std::cout << "  get_service<IPlugin>(cache): "
            << (manager.get_service<IPlugin>("cache") ? "found" : "null")
            << "\n";

  std::cout << "\n=== Query with cache disabled (only DB fires) ===\n";
  manager.event_bus().publish("query", "SELECT 1");

  std::cout << "\n=== Re-enable CachePlugin ===\n";
  manager.enable("CachePlugin");
  manager.event_bus().publish("query", "SELECT 1");

  // === 6. Conflict detection (B4) ===
  std::cout << "\n=== Conflict detection ===\n";
  try {
    manager.add_plugin(std::make_shared<ConflictingCachePlugin>(),
                       make_entry("ConflictingCache", "alt_cache"));
    std::cout << "  ERROR: should have thrown\n";
  } catch (const std::runtime_error& e) {
    std::cout << "  Caught: " << e.what() << "\n";
  }

  // === Cleanup ===
  std::cout << "\n=== Shutdown ===\n";
  manager.shutdown();

  return 0;
}
