// Umbrella header — includes all public plugin_arch headers.
// Use individual headers for fine-grained control.

#pragma once

// --- Core ---
#include "IPlugin.hpp"
#include "PluginExport.hpp"
#include "PluginLoader.hpp"
#include "DynamicLibrary.hpp"
#include "PluginDescriptor.hpp"
#include "DynamicPluginAdapter.hpp"
#include "SemVer.hpp"

// --- Discovery & Communication ---
#include "PluginRegistry.hpp"
#include "ServiceLocator.hpp"
#include "EventBus.hpp"

// --- Hot-Reload ---
#include "HotPluginLoader.hpp"

// --- Opt-in Mixins ---
#include "IServiceAware.hpp"
#include "IEventAware.hpp"
#include "ILifecycleAware.hpp"
#include "IConfigurable.hpp"
#include "IDependencyAware.hpp"
#include "IPluginMetadata.hpp"
#include "ISerializable.hpp"
#include "IHealthAware.hpp"
#include "IConflictAware.hpp"
#include "IConfigSchema.hpp"

// --- Orchestration ---
#include "PluginObserver.hpp"
#include "PluginManager.hpp"

// --- Errors ---
#include "PluginError.hpp"

// --- Thread Safety ---
#include "ThreadSafe.hpp"

// --- Testing ---
#include "MockPluginLoader.hpp"

// --- Platform ---
#include "platform/extern_c.hpp"
#include "platform/visibility.hpp"
#include "platform/shared_lib.hpp"

// clr_helpers.hpp is MSVC /clr only — not included by default.
// Include it explicitly if you need .NET Framework interop:
//   #include "platform/clr_helpers.hpp"
