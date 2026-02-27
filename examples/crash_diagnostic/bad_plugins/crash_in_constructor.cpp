// Bad plugin #1: crashes during construction (inside allocator).
// Simulates a library that segfaults when the app tries to instantiate it.

#include <stdexcept>

class CrashInConstructor {
 public:
  CrashInConstructor() {
    throw std::runtime_error("constructor failed: missing config file");
  }
};

extern "C" {
  __attribute__((visibility("default")))
  CrashInConstructor* allocator() { return new CrashInConstructor(); }

  __attribute__((visibility("default")))
  void deallocator(CrashInConstructor* ptr) { delete ptr; }
}
