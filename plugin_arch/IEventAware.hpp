// Opt-in mixin for plugins that need access to the event bus.
// Same pattern as IServiceAware — host detects via dynamic_cast and injects.

#pragma once

namespace plugin_arch {

class EventBus;

class IEventAware {
 public:
  virtual ~IEventAware() = default;
  virtual void set_event_bus(EventBus& bus) = 0;
};

}  // namespace plugin_arch
