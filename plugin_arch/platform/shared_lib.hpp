// Platform-specific shared library extension, detection, and discovery.

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

// Find the first shared library in `dir` whose filename contains `name`.
// Returns an empty path if no match is found.
inline std::filesystem::path find_plugin(const std::filesystem::path& dir,
                                         const std::string& name) {
  auto ext = shared_lib_extension();
  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    if (entry.path().extension() == ext &&
        entry.path().filename().string().find(name) != std::string::npos) {
      return entry.path();
    }
  }
  return {};
}

}  // namespace plugin_arch::platform
