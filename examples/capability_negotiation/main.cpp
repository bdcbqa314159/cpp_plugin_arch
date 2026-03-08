// Capability negotiation — shows how a host checks for required
// capabilities before loading dependent plugins.
//
// Pattern: use plugins_with_capability() to verify prerequisites,
// then use disable_group() / enable_group() for bulk operations.

#include <iostream>
#include <string>

#include "IDependencyAware.hpp"
#include "ILifecycleAware.hpp"
#include "IPluginMetadata.hpp"
#include "PluginManager.hpp"

using namespace plugin_arch;

// --- Demo plugins with capabilities ---

class PostgresPlugin : public IPlugin,
                       public ILifecycleAware,
                       public IPluginMetadata {
 public:
  const std::string& name() const override {
    static const std::string n = "PostgresPlugin";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "postgres";
    return t;
  }
  void on_init() override {
    std::cout << "  [Postgres] Connected\n";
  }
  void on_shutdown() override {
    std::cout << "  [Postgres] Disconnected\n";
  }

  const std::string& author() const override {
    static const std::string a = "DBTeam";
    return a;
  }
  const std::string& description() const override {
    static const std::string d = "PostgreSQL adapter";
    return d;
  }
  const std::string& license() const override {
    static const std::string l = "MIT";
    return l;
  }
  std::vector<std::string> capabilities() const override {
    return {"storage", "sql", "transactions"};
  }
};

class RedisPlugin : public IPlugin,
                    public ILifecycleAware,
                    public IPluginMetadata {
 public:
  const std::string& name() const override {
    static const std::string n = "RedisPlugin";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "redis";
    return t;
  }
  void on_init() override { std::cout << "  [Redis] Connected\n"; }
  void on_shutdown() override { std::cout << "  [Redis] Disconnected\n"; }

  const std::string& author() const override {
    static const std::string a = "CacheTeam";
    return a;
  }
  const std::string& description() const override {
    static const std::string d = "Redis cache adapter";
    return d;
  }
  const std::string& license() const override {
    static const std::string l = "MIT";
    return l;
  }
  std::vector<std::string> capabilities() const override {
    return {"storage", "cache"};
  }
};

class SearchPlugin : public IPlugin,
                     public ILifecycleAware,
                     public IPluginMetadata {
 public:
  const std::string& name() const override {
    static const std::string n = "SearchPlugin";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "2.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "search";
    return t;
  }
  void on_init() override { std::cout << "  [Search] Index loaded\n"; }
  void on_shutdown() override { std::cout << "  [Search] Index closed\n"; }

  const std::string& author() const override {
    static const std::string a = "SearchTeam";
    return a;
  }
  const std::string& description() const override {
    static const std::string d = "Full-text search engine";
    return d;
  }
  const std::string& license() const override {
    static const std::string l = "Apache-2.0";
    return l;
  }
  std::vector<std::string> capabilities() const override {
    return {"search", "indexing"};
  }
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

// --- Host-side capability check ---

static bool require_capability(const PluginManager& manager,
                               const std::string& capability,
                               const std::string& reason) {
  auto providers = manager.plugins_with_capability(capability);
  if (providers.empty()) {
    std::cout << "  [BLOCKED] No plugin provides '" << capability
              << "' (needed for: " << reason << ")\n";
    return false;
  }
  std::cout << "  [OK] Capability '" << capability << "' provided by: ";
  for (std::size_t i = 0; i < providers.size(); ++i) {
    if (i > 0) std::cout << ", ";
    std::cout << providers[i];
  }
  std::cout << "\n";
  return true;
}

// --- Main ---

int main() {
  PluginManager manager;

  // Load plugins
  std::cout << "=== Loading plugins ===\n";
  manager.add_plugin(std::make_shared<PostgresPlugin>(),
                     make_entry("PostgresPlugin", "postgres"));
  manager.add_plugin(std::make_shared<RedisPlugin>(),
                     make_entry("RedisPlugin", "redis"));
  manager.add_plugin(std::make_shared<SearchPlugin>(),
                     make_entry("SearchPlugin", "search", "2.0.0"));

  // Check capabilities before proceeding
  std::cout << "\n=== Capability negotiation ===\n";
  bool has_storage = require_capability(manager, "storage", "data persistence");
  bool has_sql = require_capability(manager, "sql", "complex queries");
  bool has_cache = require_capability(manager, "cache", "response caching");
  bool has_search = require_capability(manager, "search", "full-text search");
  bool has_auth = require_capability(manager, "auth", "user authentication");

  if (has_storage && has_sql && has_cache && has_search) {
    std::cout << "\n  All required capabilities available.\n";
  }
  if (!has_auth) {
    std::cout << "  Auth not available — running without authentication.\n";
  }

  // Query all storage providers
  std::cout << "\n=== Storage providers ===\n";
  auto storage_plugins = manager.plugins_with_capability("storage");
  for (const auto& name : storage_plugins) {
    std::cout << "  - " << name << "\n";
  }

  // Disable all storage plugins at once
  std::cout << "\n=== Disable all storage plugins ===\n";
  manager.disable_group("storage");
  std::cout << "  Postgres enabled? " << manager.is_enabled("PostgresPlugin")
            << "\n";
  std::cout << "  Redis enabled? " << manager.is_enabled("RedisPlugin") << "\n";

  // Re-enable them
  std::cout << "\n=== Re-enable storage group ===\n";
  manager.enable_group("storage");

  std::cout << "\n=== Shutdown ===\n";
  manager.shutdown();

  return 0;
}
