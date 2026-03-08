// Config hot-reload — shows how to reconfigure a plugin at runtime
// without reloading the shared library.
//
// Pattern: disable() → update config → enable(name, new_config)
// The plugin gets on_shutdown(), then fresh configure() + on_init().

#include <iostream>
#include <string>

#include "IConfigSchema.hpp"
#include "IConfigurable.hpp"
#include "ILifecycleAware.hpp"
#include "PluginManager.hpp"

using namespace plugin_arch;

// --- Demo plugin with config schema ---

class LoggerPlugin : public IPlugin,
                     public ILifecycleAware,
                     public IConfigurable,
                     public IConfigSchema {
 public:
  const std::string& name() const override {
    static const std::string n = "LoggerPlugin";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "1.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "logger";
    return t;
  }

  void configure(const PluginConfig& config) override {
    config_ = config;
    std::cout << "  [Logger] Configured: level=" << config_.at("level")
              << ", output=" << config_.at("output") << "\n";
  }

  void on_init() override {
    std::cout << "  [Logger] Initialized with level=" << config_.at("level")
              << "\n";
  }

  void on_shutdown() override {
    std::cout << "  [Logger] Shut down\n";
  }

  std::vector<ConfigKeyDef> config_schema() const override {
    return {
        {"level", false, "info", "Log level (debug, info, warn, error)"},
        {"output", false, "stdout", "Output destination"},
        {"max_size", false, "10MB", "Max log file size"},
    };
  }

  const PluginConfig& current_config() const { return config_; }

 private:
  PluginConfig config_;
};

// --- Helper to print current config ---

static void print_config(const LoggerPlugin& plugin) {
  std::cout << "  Current config: {";
  bool first = true;
  for (const auto& [k, v] : plugin.current_config()) {
    if (!first) std::cout << ", ";
    std::cout << k << "=" << v;
    first = false;
  }
  std::cout << "}\n";
}

// --- Main ---

int main() {
  PluginManager manager;
  auto logger = std::make_shared<LoggerPlugin>();

  // Initial load with default config (schema applies defaults)
  std::cout << "=== Initial load (defaults from schema) ===\n";
  PluginEntry entry;
  entry.name = "LoggerPlugin";
  entry.type = "logger";
  entry.version = "1.0.0";
  manager.add_plugin(logger, entry);
  print_config(*logger);

  // Hot-reconfigure: bump to debug level, redirect to file
  std::cout << "\n=== Reconfigure: level=debug, output=app.log ===\n";
  PluginManager::ConfigMap new_config = {
      {"LoggerPlugin",
       {{"level", "debug"}, {"output", "app.log"}, {"max_size", "50MB"}}}};

  manager.disable("LoggerPlugin");
  manager.enable("LoggerPlugin", new_config);
  print_config(*logger);

  // Another reconfigure: production settings
  std::cout << "\n=== Reconfigure: level=error, output=syslog ===\n";
  PluginManager::ConfigMap prod_config = {
      {"LoggerPlugin",
       {{"level", "error"}, {"output", "syslog"}}}};

  manager.disable("LoggerPlugin");
  manager.enable("LoggerPlugin", prod_config);
  print_config(*logger);

  std::cout << "\n=== Shutdown ===\n";
  manager.shutdown();

  return 0;
}
