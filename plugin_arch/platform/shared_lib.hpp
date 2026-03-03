// Platform-specific shared library extension, detection, and discovery.

#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace plugin_arch::platform {

[[nodiscard]] constexpr std::string_view shared_lib_extension() {
#if defined(_WIN32)
  return ".dll";
#elif defined(__APPLE__)
  return ".dylib";
#else
  return ".so";
#endif
}

[[nodiscard]] inline bool is_shared_library(const std::filesystem::path& path) {
  return path.extension() == shared_lib_extension();
}

// Find the first shared library in `dir` whose filename contains `name`.
// Returns an empty path if no match is found or if the directory is inaccessible.
[[nodiscard]] inline std::filesystem::path find_plugin(
    const std::filesystem::path& dir, const std::string& name) {
  auto ext = shared_lib_extension();
  std::error_code ec;
  auto it = std::filesystem::directory_iterator(dir, ec);
  if (ec) return {};
  auto end = std::filesystem::directory_iterator();
  for (; it != end; it.increment(ec)) {
    if (ec) return {};
    if (it->path().extension() == ext &&
        it->path().filename().string().find(name) != std::string::npos) {
      return it->path();
    }
  }
  return {};
}

}  // namespace plugin_arch::platform
