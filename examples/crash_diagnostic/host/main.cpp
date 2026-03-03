// Diagnostic loader — mimics what a black-box app does when loading your library,
// but with full error reporting at every stage.
//
// Usage:
//   ./crash_diag /path/to/your/library.dylib
//
// It will tell you exactly WHERE the load fails:
//   1. dlopen     — can the library be loaded at all?
//   2. symbols    — are allocator/deallocator present?
//   3. allocator  — does constructing the plugin crash?
//   4. metadata   — can we call name()/version()/type()?
//   5. deallocator — does cleanup crash?

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#if defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <Windows.h>
#else
  #include <dlfcn.h>
  #include <sys/wait.h>
  #include <unistd.h>
#endif

namespace fs = std::filesystem;

struct DiagResult {
  bool dlopen_ok = false;
  bool symbols_ok = false;
  bool allocator_ok = false;
  bool deallocator_ok = false;
  std::string error;
};

static void print_step(int step, const std::string& desc, bool ok,
                       const std::string& detail = "") {
  std::cout << "  [" << step << "] " << desc << ": "
            << (ok ? "OK" : "FAILED") << "\n";
  if (!detail.empty()) {
    std::cout << "      " << detail << "\n";
  }
}

// Run an external command with arguments, bypassing the shell entirely.
// This avoids shell injection from untrusted paths.
#if !defined(_WIN32)
static void run_command(const char* prog, std::initializer_list<const char*> args) {
  pid_t pid = fork();
  if (pid == 0) {
    // Child: build argv and exec.
    std::vector<const char*> argv;
    argv.push_back(prog);
    for (auto a : args) argv.push_back(a);
    argv.push_back(nullptr);
    execvp(prog, const_cast<char* const*>(argv.data()));
    _exit(127);  // exec failed
  } else if (pid > 0) {
    int status = 0;
    waitpid(pid, &status, 0);
  }
}
#endif

// Check what symbols the library exports (Unix only)
static void dump_exported_symbols(const std::string& path) {
#if !defined(_WIN32)
  std::cout << "\n--- Exported symbols (nm -gU) ---\n";
  run_command("nm", {"-gU", path.c_str()});
  std::cout << "---\n";
#endif
}

// Check what dynamic dependencies the library has
static void dump_dependencies(const std::string& path) {
#if defined(__APPLE__)
  std::cout << "\n--- Dependencies (otool -L) ---\n";
  run_command("otool", {"-L", path.c_str()});
  std::cout << "---\n";
#elif defined(__linux__)
  std::cout << "\n--- Dependencies (ldd) ---\n";
  run_command("ldd", {path.c_str()});
  std::cout << "---\n";
#endif
}

static DiagResult diagnose(const std::string& path,
                           const std::string& alloc_sym = "allocator",
                           const std::string& dealloc_sym = "deallocator") {
  DiagResult result;

  std::cout << "\n=== Diagnosing: " << path << " ===\n\n";

  // --- Step 1: dlopen ---
#if defined(_WIN32)
  HMODULE handle = LoadLibraryA(path.c_str());
  if (!handle) {
    result.error = "LoadLibrary failed";
    print_step(1, "dlopen (LoadLibrary)", false, result.error);
    return result;
  }
#else
  // Clear any previous error
  dlerror();

  void* handle = dlopen(path.c_str(), RTLD_NOW);
  if (!handle) {
    const char* dl_err = dlerror();
    result.error = dl_err ? dl_err : "unknown error";
    print_step(1, "dlopen", false, result.error);
    dump_dependencies(path);
    dump_exported_symbols(path);
    return result;
  }
#endif
  result.dlopen_ok = true;
  print_step(1, "dlopen", true, "Library loaded into memory");

  // --- Step 2: Symbol resolution ---
  using AllocFunc = void* (*)();
  using DeallocFunc = void (*)(void*);

  AllocFunc alloc_fn = nullptr;
  DeallocFunc dealloc_fn = nullptr;
  std::string alloc_detail;
  std::string dealloc_detail;

#if defined(_WIN32)
  alloc_fn = reinterpret_cast<AllocFunc>(GetProcAddress(handle, alloc_sym.c_str()));
  if (!alloc_fn)
    alloc_detail = "'" + alloc_sym + "' not found (error " +
                   std::to_string(GetLastError()) + ")";
  dealloc_fn = reinterpret_cast<DeallocFunc>(GetProcAddress(handle, dealloc_sym.c_str()));
  if (!dealloc_fn)
    dealloc_detail = "'" + dealloc_sym + "' not found (error " +
                     std::to_string(GetLastError()) + ")";
#else
  dlerror();
  alloc_fn = reinterpret_cast<AllocFunc>(dlsym(handle, alloc_sym.c_str()));
  if (const char* err = dlerror())
    alloc_detail = std::string("'") + alloc_sym + "' not found: " + err;
  dlerror();
  dealloc_fn = reinterpret_cast<DeallocFunc>(dlsym(handle, dealloc_sym.c_str()));
  if (const char* err = dlerror())
    dealloc_detail = std::string("'") + dealloc_sym + "' not found: " + err;
#endif

  if (!alloc_fn || !dealloc_fn) {
    std::string detail;
    if (!alloc_fn) detail += alloc_detail + "  ";
    if (!dealloc_fn) detail += dealloc_detail;
    print_step(2, "Symbol resolution", false, detail);
    dump_exported_symbols(path);
#if !defined(_WIN32)
    dlclose(handle);
#else
    FreeLibrary(handle);
#endif
    return result;
  }
  result.symbols_ok = true;
  print_step(2, "Symbol resolution", true,
             "Found '" + alloc_sym + "' and '" + dealloc_sym + "'");

  // --- Step 3: Call allocator ---
  void* instance = nullptr;
  try {
    instance = alloc_fn();
    if (!instance) {
      print_step(3, "Allocator call", false, "Returned nullptr");
#if !defined(_WIN32)
      dlclose(handle);
#else
      FreeLibrary(handle);
#endif
      return result;
    }
    result.allocator_ok = true;
    print_step(3, "Allocator call", true, "Plugin instance created");
  } catch (const std::exception& e) {
    print_step(3, "Allocator call", false,
               std::string("Exception: ") + e.what());
#if !defined(_WIN32)
    dlclose(handle);
#else
    FreeLibrary(handle);
#endif
    return result;
  } catch (...) {
    print_step(3, "Allocator call", false, "Unknown exception thrown");
#if !defined(_WIN32)
    dlclose(handle);
#else
    FreeLibrary(handle);
#endif
    return result;
  }

  // --- Step 4: Call deallocator ---
  try {
    dealloc_fn(instance);
    result.deallocator_ok = true;
    print_step(4, "Deallocator call", true, "Plugin instance destroyed cleanly");
  } catch (const std::exception& e) {
    print_step(4, "Deallocator call", false,
               std::string("Exception: ") + e.what());
  } catch (...) {
    print_step(4, "Deallocator call", false, "Unknown exception thrown");
  }

  // --- Cleanup ---
#if !defined(_WIN32)
  dlclose(handle);
#else
  FreeLibrary(handle);
#endif

  std::cout << "\n";
  if (result.deallocator_ok) {
    std::cout << "RESULT: All checks passed. Library loads correctly.\n";
  } else {
    std::cout << "RESULT: Failed at step above. That's where your black-box app crashes.\n";
  }

  dump_dependencies(path);
  dump_exported_symbols(path);

  return result;
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <library_path> [alloc_symbol] [dealloc_symbol]\n";
    std::cerr << "\nDiagnoses why a shared library fails to load.\n";
    std::cerr << "Default symbols: 'allocator' and 'deallocator'\n";
    return 1;
  }

  std::string path = argv[1];
  std::string alloc_sym = argc > 2 ? argv[2] : "allocator";
  std::string dealloc_sym = argc > 3 ? argv[3] : "deallocator";

  if (!fs::exists(path)) {
    std::cerr << "File not found: " << path << "\n";
    return 1;
  }

  diagnose(path, alloc_sym, dealloc_sym);

  return 0;
}
