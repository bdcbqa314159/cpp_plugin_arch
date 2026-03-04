// Drop-in replacement for PluginLoader<T> that returns a user-provided
// instance without touching dlopen. Lets you unit test host logic
// (registry queries, service locator wiring, lifecycle ordering) in isolation.
//
// Same public API as PluginLoader<T>:
//   - get_instance() returns shared_ptr<T>
//   - path() returns the library path string
//
// Host code templated on the loader type can use either:
//   template <typename Loader>
//   void setup(Loader& loader) { auto inst = loader.get_instance(); ... }

#pragma once

#include <memory>
#include <string>

namespace plugin_arch {

template <typename T>
class MockPluginLoader {
 public:
  explicit MockPluginLoader(std::shared_ptr<T> instance,
                            std::string path = "mock")
      : instance_(std::move(instance)),
        path_(std::move(path)) {}

  [[nodiscard]] std::shared_ptr<T> get_instance() { return instance_; }

  [[nodiscard]] const std::string& path() const { return path_; }

 private:
  std::shared_ptr<T> instance_;
  std::string path_;
};

// Drop-in replacement for HotPluginLoader<T>.
// Adds set_instance() to simulate reloads in tests.
template <typename T>
class MockHotPluginLoader {
 public:
  explicit MockHotPluginLoader(std::shared_ptr<T> instance,
                               std::string library_path = "mock")
      : instance_(std::move(instance)),
        library_path_(std::move(library_path)) {}

  [[nodiscard]] std::shared_ptr<T> get_instance() const { return instance_; }

  // Simulate a reload by swapping in a new instance.
  void set_instance(std::shared_ptr<T> instance) {
    instance_ = std::move(instance);
  }

  // Always returns false — use set_instance() to trigger "reloads".
  [[nodiscard]] bool check_and_reload() { return false; }

  void reload() {}

  [[nodiscard]] const std::string& library_path() const { return library_path_; }

 private:
  std::shared_ptr<T> instance_;
  std::string library_path_;
};

}  // namespace plugin_arch
