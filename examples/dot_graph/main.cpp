// DOT graph export — shows how to convert dependency_graph() output
// into Graphviz DOT format for visualization.
//
// Pattern: parse dependency_graph() strings, emit DOT syntax.
// Pipe output to: dot -Tpng -o deps.png

#include <iostream>
#include <string>

#include "IDependencyAware.hpp"
#include "ILifecycleAware.hpp"
#include "PluginManager.hpp"

using namespace plugin_arch;

// --- Demo plugins forming a dependency chain ---

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
  void on_init() override {}
  void on_shutdown() override {}
};

class StoragePlugin : public IPlugin,
                      public ILifecycleAware,
                      public IDependencyAware {
 public:
  const std::string& name() const override {
    static const std::string n = "Storage";
    return n;
  }
  const std::string& version() const override {
    static const std::string v = "2.0.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "storage";
    return t;
  }
  std::vector<std::string> dependencies() const override { return {"core"}; }
  void on_init() override {}
  void on_shutdown() override {}
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
    static const std::string v = "1.5.0";
    return v;
  }
  const std::string& type() const override {
    static const std::string t = "auth";
    return t;
  }
  std::vector<std::string> dependencies() const override { return {"core"}; }
  void on_init() override {}
  void on_shutdown() override {}
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
    return {"storage", "auth"};
  }
  void on_init() override {}
  void on_shutdown() override {}
};

// --- DOT export function (host-side code) ---

// Escape a string for use inside DOT double-quoted identifiers.
static std::string dot_escape(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  return out;
}

static void export_dot(const PluginManager& manager, std::ostream& out) {
  out << "digraph plugins {\n";
  out << "  rankdir=BT;\n";
  out << "  node [shape=box, style=filled, fillcolor=lightblue];\n\n";

  for (const auto& lp : manager.loaded_plugins()) {
    auto ename = dot_escape(lp.entry.name);
    auto eversion = dot_escape(lp.entry.version);

    // Node label with name and version
    out << "  \"" << ename << "\" [label=\"" << ename
        << "\\nv" << eversion << "\""
        << (lp.enabled ? "" : ", fillcolor=gray") << "];\n";

    // Edges: plugin → each dependency
    for (const auto& dep_type : lp.deps) {
      // Find the plugin name that provides this type
      for (const auto& provider : manager.loaded_plugins()) {
        if (provider.entry.type == dep_type) {
          out << "  \"" << ename << "\" -> \""
              << dot_escape(provider.entry.name) << "\";\n";
          break;
        }
      }
    }
  }

  out << "}\n";
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

// --- Main ---

int main() {
  PluginManager manager;

  manager.add_plugin(std::make_shared<CorePlugin>(),
                     make_entry("Core", "core"));
  manager.add_plugin(std::make_shared<StoragePlugin>(),
                     make_entry("Storage", "storage", "2.0.0"));
  manager.add_plugin(std::make_shared<AuthPlugin>(),
                     make_entry("Auth", "auth", "1.5.0"));
  manager.add_plugin(std::make_shared<ApiPlugin>(),
                     make_entry("Api", "api", "3.0.0"));

  // Text format (from library)
  std::cout << "── dependency_graph() output ──\n\n";
  for (const auto& line : manager.dependency_graph()) {
    std::cout << "  " << line << "\n";
  }

  // DOT format (host-side conversion)
  std::cout << "\n── Graphviz DOT output ──\n";
  std::cout << "── (pipe to: dot -Tpng -o deps.png) ──\n\n";
  export_dot(manager, std::cout);

  manager.shutdown();
  return 0;
}
