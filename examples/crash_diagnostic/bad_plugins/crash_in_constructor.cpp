// Bad plugin #1: crashes during construction (inside allocator).
// Simulates a library that segfaults when the app tries to instantiate it.

// Bad plugin #1: crashes during construction (inside allocator).
// Simulates a library that segfaults when the app tries to instantiate it.

#include <stdexcept>

#if defined(_WIN32)
  #define BAD_EXPORT __declspec(dllexport)
#else
  #define BAD_EXPORT __attribute__((visibility("default")))
#endif

class CrashInConstructor {
 public:
  CrashInConstructor() {
    throw std::runtime_error("constructor failed: missing config file");
  }
};

extern "C" {
  BAD_EXPORT CrashInConstructor* allocator() { return new CrashInConstructor(); }
  BAD_EXPORT void deallocator(CrashInConstructor* ptr) { delete ptr; }
}
