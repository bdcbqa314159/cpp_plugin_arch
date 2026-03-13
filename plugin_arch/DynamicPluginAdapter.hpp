// Adapter that wraps a C ABI plugin (PluginDescriptor) as an IPlugin.
//
// Promotes "second-class" C ABI plugins to first-class framework citizens
// with lifecycle hooks, configuration, and service discovery.
//
// Optional C ABI callbacks detected via dlsym:
//   extern "C" EXPORTED void plugin_init();
//   extern "C" EXPORTED void plugin_shutdown();
//   extern "C" EXPORTED void plugin_configure(int count,
//                                              const char** keys,
//                                              const char** values);
//
// Usage:
//   auto adapter = DynamicPluginAdapter::load("path/to/plugin.so");
//   auto func = adapter->get_function<int(*)(int,int)>("math_add");
//   int result = func(2, 3);

#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "DynamicLibrary.hpp"
#include "IConfigurable.hpp"
#include "ILifecycleAware.hpp"
#include "IPlugin.hpp"
#include "PluginDescriptor.hpp"

namespace plugin_arch {

class DynamicPluginAdapter : public IPlugin,
                             public ILifecycleAware,
                             public IConfigurable {
 public:
  using InitFunc = void (*)();
  using ShutdownFunc = void (*)();
  using ConfigureFunc = void (*)(int count, const char** keys,
                                 const char** values);

  [[nodiscard]] static std::shared_ptr<DynamicPluginAdapter> load(
      const std::string& path) {
    return std::make_shared<DynamicPluginAdapter>(path);
  }

  explicit DynamicPluginAdapter(const std::string& path)
      : lib_(std::make_shared<DynamicLibrary>(path)) {
    auto describe =
        lib_->resolve<const PluginDescriptor* (*)()>("plugin_describe");
    desc_ = describe();
    if (!desc_) {
      throw std::runtime_error("plugin_describe() returned nullptr: " + path);
    }

    name_ = desc_->name ? desc_->name : "";
    version_ = desc_->version ? desc_->version : "";
    // Use plugin name as type — host can override if needed
    type_ = name_;

    // Probe for optional lifecycle callbacks
    if (lib_->has("plugin_init")) {
      init_func_ = lib_->resolve<InitFunc>("plugin_init");
    }
    if (lib_->has("plugin_shutdown")) {
      shutdown_func_ = lib_->resolve<ShutdownFunc>("plugin_shutdown");
    }
    if (lib_->has("plugin_configure")) {
      configure_func_ = lib_->resolve<ConfigureFunc>("plugin_configure");
    }
  }

  // IPlugin
  const std::string& name() const override { return name_; }
  const std::string& version() const override { return version_; }
  const std::string& type() const override { return type_; }

  // ILifecycleAware
  void on_init() override {
    if (init_func_) init_func_();
  }

  void on_shutdown() override {
    if (shutdown_func_) shutdown_func_();
  }

  // IConfigurable
  void configure(const PluginConfig& config) override {
    if (!configure_func_) return;

    std::vector<const char*> keys;
    std::vector<const char*> values;
    keys.reserve(config.size());
    values.reserve(config.size());

    for (const auto& [k, v] : config) {
      keys.push_back(k.c_str());
      values.push_back(v.c_str());
    }
    // C ABI uses int for count — safe as long as config has < INT_MAX entries.
    static_assert(sizeof(int) >= 4, "int must be at least 32-bit");
    configure_func_(static_cast<int>(keys.size()), keys.data(), values.data());
  }

  // Access C ABI functions by name.
  template <typename Func>
  [[nodiscard]] Func get_function(const std::string& func_name) const {
    return lib_->resolve<Func>(func_name);
  }

  [[nodiscard]] bool has_function(const std::string& func_name) const {
    return lib_->has(func_name);
  }

  // WARNING: function_count and functions come from the plugin — the framework
  // cannot validate that function_count matches the actual array length.
  // Callers iterating functions must not trust function_count blindly if the
  // plugin source is untrusted.
  [[nodiscard]] const PluginDescriptor& descriptor() const { return *desc_; }

  [[nodiscard]] const std::shared_ptr<DynamicLibrary>& library() const {
    return lib_;
  }

 private:
  std::shared_ptr<DynamicLibrary> lib_;
  const PluginDescriptor* desc_ = nullptr;
  std::string name_;
  std::string version_;
  std::string type_;

  InitFunc init_func_ = nullptr;
  ShutdownFunc shutdown_func_ = nullptr;
  ConfigureFunc configure_func_ = nullptr;
};

}  // namespace plugin_arch
