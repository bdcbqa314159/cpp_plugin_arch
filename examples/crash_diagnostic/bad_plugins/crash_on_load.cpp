// Bad plugin #3: crashes at dlopen time, before any symbol is resolved.
// A global/static constructor runs during library loading and fails.
// This is the hardest to debug because it happens inside dlopen() itself.

// Bad plugin #3: crashes at dlopen time, before any symbol is resolved.
// A global/static constructor runs during library loading and fails.

#include <stdexcept>

#if defined(_WIN32)
  #define BAD_EXPORT __declspec(dllexport)
#else
  #define BAD_EXPORT __attribute__((visibility("default")))
#endif

static int bad_init() {
  throw std::runtime_error("static init failed: cannot connect to server");
  return 0;
}

// This runs when the library is loaded — before allocator() is ever called
static int force_init = bad_init();

class CrashOnLoad {
 public:
  int value() { return force_init; }
};

extern "C" {
  BAD_EXPORT CrashOnLoad* allocator() { return new CrashOnLoad(); }
  BAD_EXPORT void deallocator(CrashOnLoad* ptr) { delete ptr; }
}
