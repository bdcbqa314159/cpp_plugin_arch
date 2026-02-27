// Bad plugin #2: missing the expected factory symbols.
// The library loads fine, but dlsym can't find allocator/deallocator.
// Common cause: forgot REGISTER_PLUGIN(), or symbols aren't extern "C".

class MissingSymbols {
 public:
  int do_something() { return 42; }
};

// Oops — no extern "C", no EXPORTED. The symbols exist but are mangled.
MissingSymbols* allocator() { return new MissingSymbols(); }
void deallocator(MissingSymbols* ptr) { delete ptr; }
