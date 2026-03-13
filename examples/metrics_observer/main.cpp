// Metrics observer — shows how to collect operational metrics
// using PluginObserver without modifying the library.
//
// Pattern: implement PluginObserver, track counts and timings,
// print a summary on demand.

#include <chrono>
#include <iostream>
#include <string>
#include <unordered_map>

#include "ILifecycleAware.hpp"
#include "PluginManager.hpp"
#include "PluginObserver.hpp"

using namespace plugin_arch;

// --- Metrics observer (host-side code) ---

class MetricsObserver : public PluginObserver {
 public:
  void on_plugin_loaded(const std::string& name,
                        const std::string&,
                        const PluginEntry&) override {
    ++total_loads_;
    ++plugin_loads_[name];
    std::cout << "  [metrics] loaded: " << name
              << " (total loads: " << total_loads_ << ")\n";
  }

  void on_plugin_unloaded(const std::string& name,
                          const std::string&,
                          const PluginEntry&) override {
    ++total_unloads_;
    std::cout << "  [metrics] unloaded: " << name << "\n";
  }

  void on_plugin_enabled(const std::string& name,
                         const std::string&,
                         const PluginEntry&) override {
    ++total_enables_;
    std::cout << "  [metrics] enabled: " << name << "\n";
  }

  void on_plugin_disabled(const std::string& name,
                          const std::string&,
                          const PluginEntry&) override {
    ++total_disables_;
    std::cout << "  [metrics] disabled: " << name << "\n";
  }

  void on_plugin_reloaded(const std::string& name,
                          const std::string&,
                          const PluginEntry&) override {
    ++total_reloads_;
    std::cout << "  [metrics] reloaded: " << name << "\n";
  }

  void print_summary() const {
    std::cout << "\n── Metrics Summary ──\n";
    std::cout << "  Total loads:    " << total_loads_ << "\n";
    std::cout << "  Total unloads:  " << total_unloads_ << "\n";
    std::cout << "  Total enables:  " << total_enables_ << "\n";
    std::cout << "  Total disables: " << total_disables_ << "\n";
    std::cout << "  Total reloads:  " << total_reloads_ << "\n";
    std::cout << "\n  Per-plugin load counts:\n";
    for (const auto& [name, count] : plugin_loads_) {
      std::cout << "    " << name << ": " << count << "\n";
    }
  }

 private:
  int total_loads_ = 0;
  int total_unloads_ = 0;
  int total_enables_ = 0;
  int total_disables_ = 0;
  int total_reloads_ = 0;
  std::unordered_map<std::string, int> plugin_loads_;
};

// --- Timed add_plugin wrapper (host-side code) ---

static PluginEntry make_entry(const std::string& name,
                              const std::string& type,
                              const std::string& version = "1.0.0") {
  PluginEntry e;
  e.name = name;
  e.type = type;
  e.version = version;
  return e;
}

static void timed_add_plugin(PluginManager& manager,
                             std::shared_ptr<IPlugin> instance,
                             const PluginEntry& entry) {
  auto start = std::chrono::steady_clock::now();
  manager.add_plugin(std::move(instance), entry);
  auto elapsed = std::chrono::steady_clock::now() - start;
  auto us = std::chrono::duration_cast<std::chrono::microseconds>(elapsed);
  std::cout << "  [timing] " << entry.name << " loaded in " << us.count()
            << " µs\n";
}

// --- Simple demo plugins ---

class AlphaPlugin : public IPlugin, public ILifecycleAware {
 public:
  const std::string& name() const override {
    static const std::string n = "Alpha";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "alpha";
    return t;
  }
  void on_init() override {}
  void on_shutdown() override {}
};

class BetaPlugin : public IPlugin, public ILifecycleAware {
 public:
  const std::string& name() const override {
    static const std::string n = "Beta";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "2.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "beta";
    return t;
  }
  void on_init() override {}
  void on_shutdown() override {}
};

// --- Main ---

int main() {
  PluginManager manager;
  MetricsObserver metrics;
  manager.add_observer(&metrics);

  std::cout << "=== Loading plugins (with timing) ===\n";
  timed_add_plugin(manager, std::make_shared<AlphaPlugin>(),
                   make_entry("Alpha", "alpha"));
  timed_add_plugin(manager, std::make_shared<BetaPlugin>(),
                   make_entry("Beta", "beta", "2.0.0"));

  std::cout << "\n=== Disable/enable cycle ===\n";
  manager.disable("Alpha");
  manager.enable("Alpha");
  manager.disable("Beta");
  manager.enable("Beta");

  std::cout << "\n=== Unload Beta ===\n";
  manager.unload("Beta");

  metrics.print_summary();

  std::cout << "\n=== Shutdown ===\n";
  manager.shutdown();

  return 0;
}
