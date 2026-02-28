// Opt-in lifecycle hooks for plugins that need post-construction setup
// and pre-destruction teardown (open files, flush buffers, print summaries).
//
// Same pattern as IServiceAware — standalone mixin, detected via dynamic_cast.
// Plugins opt in through multiple inheritance.

#pragma once

namespace plugin_arch {

class ILifecycleAware {
 public:
  virtual ~ILifecycleAware() = default;

  // Called by the host after construction (and after service injection, if any).
  virtual void on_init() = 0;

  // Called by the host before destruction (flush buffers, print summaries).
  virtual void on_shutdown() = 0;
};

}  // namespace plugin_arch
