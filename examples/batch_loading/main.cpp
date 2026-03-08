// Batch loading — shows how a host loads a set of plugins from a
// manifest, respecting dependency order via topological sort.
//
// Pattern: define a manifest of plugin entries, topo-sort by
// dependencies, then call add_plugin() in order.

#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "IDependencyAware.hpp"
#include "ILifecycleAware.hpp"
#include "PluginManager.hpp"

using namespace plugin_arch;

// --- Demo plugins forming a dependency DAG ---

class CorePlugin : public IPlugin, public ILifecycleAware {
 public:
  const std::string& name() const override {
    static const std::string n = "Core";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "core";
    return t;
  }
  void on_init() override { std::cout << "  [Core] Ready\n"; }
  void on_shutdown() override { std::cout << "  [Core] Stopped\n"; }
};

class LoggingPlugin : public IPlugin,
                      public ILifecycleAware,
                      public IDependencyAware {
 public:
  const std::string& name() const override {
    static const std::string n = "Logging";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "logging";
    return t;
  }
  std::vector<std::string> dependencies() const override { return {"core"}; }
  void on_init() override { std::cout << "  [Logging] Ready\n"; }
  void on_shutdown() override { std::cout << "  [Logging] Stopped\n"; }
};

class DatabasePlugin : public IPlugin,
                       public ILifecycleAware,
                       public IDependencyAware {
 public:
  const std::string& name() const override {
    static const std::string n = "Database";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "2.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "database";
    return t;
  }
  std::vector<std::string> dependencies() const override {
    return {"core", "logging"};
  }
  void on_init() override { std::cout << "  [Database] Ready\n"; }
  void on_shutdown() override { std::cout << "  [Database] Stopped\n"; }
};

class AuthPlugin : public IPlugin,
                   public ILifecycleAware,
                   public IDependencyAware {
 public:
  const std::string& name() const override {
    static const std::string n = "Auth";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "auth";
    return t;
  }
  std::vector<std::string> dependencies() const override {
    return {"database"};
  }
  void on_init() override { std::cout << "  [Auth] Ready\n"; }
  void on_shutdown() override { std::cout << "  [Auth] Stopped\n"; }
};

class ApiPlugin : public IPlugin,
                  public ILifecycleAware,
                  public IDependencyAware {
 public:
  const std::string& name() const override {
    static const std::string n = "Api";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "3.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "api";
    return t;
  }
  std::vector<std::string> dependencies() const override {
    return {"auth", "database", "logging"};
  }
  void on_init() override { std::cout << "  [Api] Ready\n"; }
  void on_shutdown() override { std::cout << "  [Api] Stopped\n"; }
};

// --- Manifest + batch loader (host-side code) ---

struct ManifestEntry {
  std::string name;
  std::string type;
  std::string version;
  std::vector<std::string> deps;  // dependency types
  std::function<std::shared_ptr<IPlugin>()> factory;
};

// Host-side topological sort of manifest entries.
// Returns entries in dependency order (leaves first).
static std::vector<ManifestEntry> topo_sort(
    const std::vector<ManifestEntry>& manifest) {
  std::unordered_map<std::string, const ManifestEntry*> by_type;
  for (const auto& m : manifest) by_type[m.type] = &m;

  std::unordered_set<std::string> visited;
  std::unordered_set<std::string> in_stack;
  std::vector<ManifestEntry> result;

  std::function<void(const ManifestEntry&)> visit =
      [&](const ManifestEntry& entry) {
        if (visited.contains(entry.type)) return;
        if (in_stack.contains(entry.type)) {
          throw std::runtime_error("Circular dependency: " + entry.type);
        }
        in_stack.insert(entry.type);
        for (const auto& dep : entry.deps) {
          auto it = by_type.find(dep);
          if (it == by_type.end()) {
            throw std::runtime_error("Missing dependency: " + dep +
                                     " (required by " + entry.type + ")");
          }
          visit(*it->second);
        }
        in_stack.erase(entry.type);
        visited.insert(entry.type);
        result.push_back(entry);
      };

  for (const auto& m : manifest) visit(m);
  return result;
}

static PluginEntry make_entry(const std::string& name,
                              const std::string& type,
                              const std::string& version = "1.0.0") {
  PluginEntry e;
  e.name = name;
  e.type = type;
  e.version = version;
  return e;
}

static void batch_load(PluginManager& manager,
                       const std::vector<ManifestEntry>& manifest) {
  auto sorted = topo_sort(manifest);

  std::cout << "  Load order: ";
  for (std::size_t i = 0; i < sorted.size(); ++i) {
    if (i > 0) std::cout << " -> ";
    std::cout << sorted[i].name;
  }
  std::cout << "\n\n";

  for (const auto& m : sorted) {
    auto instance = m.factory();
    manager.add_plugin(instance, make_entry(m.name, m.type, m.version));
  }
}

// --- Main ---

int main() {
  PluginManager manager;

  // Define manifest (deliberately out of order)
  std::vector<ManifestEntry> manifest = {
      {"Api", "api", "3.0.0", {"auth", "database", "logging"},
       [] { return std::make_shared<ApiPlugin>(); }},
      {"Core", "core", "1.0.0", {},
       [] { return std::make_shared<CorePlugin>(); }},
      {"Auth", "auth", "1.0.0", {"database"},
       [] { return std::make_shared<AuthPlugin>(); }},
      {"Database", "database", "2.0.0", {"core", "logging"},
       [] { return std::make_shared<DatabasePlugin>(); }},
      {"Logging", "logging", "1.0.0", {"core"},
       [] { return std::make_shared<LoggingPlugin>(); }},
  };

  std::cout << "=== Manifest (unordered) ===\n";
  for (const auto& m : manifest) {
    std::cout << "  " << m.name << " v" << m.version;
    if (!m.deps.empty()) {
      std::cout << " (needs: ";
      for (std::size_t i = 0; i < m.deps.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << m.deps[i];
      }
      std::cout << ")";
    }
    std::cout << "\n";
  }

  std::cout << "\n=== Batch loading (topo-sorted) ===\n";
  batch_load(manager, manifest);

  // Show dependency graph from the library
  std::cout << "=== Dependency graph ===\n";
  for (const auto& line : manager.dependency_graph()) {
    std::cout << "  " << line << "\n";
  }

  std::cout << "\n=== Shutdown ===\n";
  manager.shutdown();

  return 0;
}
