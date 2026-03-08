// Diagnostic dump — shows how a host builds a full system report
// from existing PluginManager query methods.
//
// Pattern: iterate loaded_plugins() + check_health() + dependency_graph()
// to produce a one-call diagnostic snapshot.

#include <iostream>
#include <string>

#include "IConfigurable.hpp"
#include "IDependencyAware.hpp"
#include "IHealthAware.hpp"
#include "ILifecycleAware.hpp"
#include "IPluginMetadata.hpp"
#include "PluginManager.hpp"

using namespace plugin_arch;

// --- Demo plugins ---

class DatabasePlugin : public IPlugin,
                       public ILifecycleAware,
                       public IHealthAware,
                       public IPluginMetadata,
                       public IConfigurable {
 public:
  const std::string& name() const override {
    static const std::string n = "DatabasePlugin";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "3.1.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "database";
    return t;
  }

  void on_init() override { connected_ = true; }
  void on_shutdown() override { connected_ = false; }

  bool is_healthy() const override { return connected_; }
  HealthStatus health_status() const override {
    return connected_ ? HealthStatus{true, "connected, 42 active queries"}
                      : HealthStatus{false, "connection lost"};
  }

  void configure(const PluginConfig& config) override { config_ = config; }

  const std::string& author() const override {
    static const std::string a = "DBTeam";
    return a;
  }
  const std::string& description() const override {
    static const std::string d = "PostgreSQL connection pool";
    return d;
  }
  const std::string& license() const override {
    static const std::string l = "MIT";
    return l;
  }
  std::vector<std::string> capabilities() const override {
    return {"storage", "sql"};
  }

  void simulate_disconnect() { connected_ = false; }

 private:
  bool connected_ = false;
  PluginConfig config_;
};

class CachePlugin : public IPlugin,
                    public ILifecycleAware,
                    public IHealthAware,
                    public IDependencyAware {
 public:
  const std::string& name() const override {
    static const std::string n = "CachePlugin";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "1.2.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "cache";
    return t;
  }

  std::vector<std::string> dependencies() const override {
    return {"database"};
  }
  void on_init() override {}
  void on_shutdown() override {}

  bool is_healthy() const override { return true; }
  HealthStatus health_status() const override {
    return {true, "hit rate: 87%"};
  }
};

class ApiPlugin : public IPlugin,
                  public ILifecycleAware,
                  public IDependencyAware {
 public:
  const std::string& name() const override {
    static const std::string n = "ApiPlugin";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "2.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "api";
    return t;
  }

  std::vector<std::string> dependencies() const override {
    return {"database", "cache"};
  }
  void on_init() override {}
  void on_shutdown() override {}
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

// --- Diagnostic dump function (host-side code) ---

static void dump_system(const PluginManager& manager) {
  std::cout << "╔══════════════════════════════════════╗\n";
  std::cout << "║       PLUGIN SYSTEM DIAGNOSTIC       ║\n";
  std::cout << "╚══════════════════════════════════════╝\n\n";

  // 1. Plugin inventory
  const auto& plugins = manager.loaded_plugins();
  std::cout << "── Loaded plugins (" << plugins.size() << ") ──\n\n";
  for (const auto& lp : plugins) {
    std::cout << "  " << lp.entry.name << " v" << lp.entry.version
              << " [type: " << lp.entry.type << "]"
              << (lp.enabled ? "" : " (DISABLED)") << "\n";

    auto* meta = dynamic_cast<IPluginMetadata*>(lp.instance.get());
    if (meta) {
      std::cout << "    author: " << meta->author()
                << "  license: " << meta->license() << "\n";
      auto caps = meta->capabilities();
      if (!caps.empty()) {
        std::cout << "    capabilities: ";
        for (std::size_t i = 0; i < caps.size(); ++i) {
          if (i > 0) std::cout << ", ";
          std::cout << caps[i];
        }
        std::cout << "\n";
      }
    }
  }

  // 2. Dependency graph
  std::cout << "\n── Dependency graph ──\n\n";
  for (const auto& line : manager.dependency_graph()) {
    std::cout << "  " << line << "\n";
  }

  // 3. Health status
  auto reports = manager.check_health(/*include_healthy=*/true);
  std::cout << "\n── Health status ──\n\n";
  if (reports.empty()) {
    std::cout << "  (no IHealthAware plugins)\n";
  }
  for (const auto& r : reports) {
    std::cout << "  " << (r.status.healthy ? "[OK]  " : "[FAIL]")
              << " " << r.plugin_name << " — " << r.status.message << "\n";
  }

  std::cout << "\n";
}

// --- Main ---

int main() {
  PluginManager manager;

  auto db = std::make_shared<DatabasePlugin>();
  PluginManager::ConfigMap config = {
      {"DatabasePlugin", {{"host", "db.prod.internal"}, {"pool_size", "20"}}}};

  manager.add_plugin(db, make_entry("DatabasePlugin", "database", "3.1.0"),
                     config);
  manager.add_plugin(std::make_shared<CachePlugin>(),
                     make_entry("CachePlugin", "cache", "1.2.0"));
  manager.add_plugin(std::make_shared<ApiPlugin>(),
                     make_entry("ApiPlugin", "api", "2.0.0"));

  std::cout << "=== All healthy ===\n\n";
  dump_system(manager);

  std::cout << "=== After database disconnect ===\n\n";
  db->simulate_disconnect();
  dump_system(manager);

  std::cout << "=== After disabling CachePlugin ===\n\n";
  manager.disable("CachePlugin");
  dump_system(manager);

  manager.shutdown();
  return 0;
}
