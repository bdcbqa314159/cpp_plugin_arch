// Platform-specific shared library extension and detection.
// Extracts the logic that was copy-pasted in every host application.

#pragma once

#include <filesystem>
#include <string>

namespace plugin_arch::platform {

inline std::string shared_lib_extension() {
#if defined(_WIN32)
  return ".dll";
#elif defined(__APPLE__)
  return ".dylib";
#else
  return ".so";
#endif
}

inline bool is_shared_library(const std::filesystem::path& path) {
  return path.extension() == shared_lib_extension();
}

}  // namespace plugin_arch::platform
