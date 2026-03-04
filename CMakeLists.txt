cmake_minimum_required(VERSION 3.20)

project(cpp_plugin_arch LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# --- plugin_arch: header-only library ---
add_library(plugin_arch INTERFACE)
target_include_directories(plugin_arch INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin_arch
)

# Link platform dynamic-loading libs
if(UNIX)
    target_link_libraries(plugin_arch INTERFACE dl)
endif()

# GCC < 9 requires linking stdc++fs for std::filesystem
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    link_libraries(stdc++fs)
endif()

# --- CMake helpers for plugins ---
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/PluginArch.cmake)

# --- examples and tests (only when building standalone) ---
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(examples)
    add_subdirectory(tests)
endif()
