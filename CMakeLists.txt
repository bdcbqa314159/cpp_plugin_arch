cmake_minimum_required(VERSION 3.20)

project(cpp_plugin_arch
    VERSION 1.0.0
    LANGUAGES CXX
    DESCRIPTION "C++20 header-only plugin architecture library"
)

# ── Default install prefix (in-project release bundle) ───────────

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/dist"
        CACHE PATH "Install path (default: <project>/dist)" FORCE)
endif()

# ── Global settings ──────────────────────────────────────────────

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Output directories (standalone builds only) ──────────────────
# Groups all executables and plugins into bin/ for easy discovery.
# Scoped to standalone builds so we don't pollute parent projects.

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
endif()

# ── Options ──────────────────────────────────────────────────────

option(PLUGIN_ARCH_BUILD_EXAMPLES "Build example programs" ON)
option(PLUGIN_ARCH_BUILD_TESTS    "Build test suite"       ON)
option(PLUGIN_ARCH_INSTALL        "Generate install target" ON)

# ── Header-only INTERFACE library ────────────────────────────────

add_library(plugin_arch INTERFACE)
add_library(plugin_arch::plugin_arch ALIAS plugin_arch)

target_include_directories(plugin_arch INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/plugin_arch>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(plugin_arch INTERFACE cxx_std_20)

# Platform dynamic-loading libs
if(UNIX AND NOT APPLE)
    target_link_libraries(plugin_arch INTERFACE ${CMAKE_DL_LIBS})
elseif(APPLE)
    target_link_libraries(plugin_arch INTERFACE ${CMAKE_DL_LIBS})
endif()

# GCC < 9 requires linking stdc++fs for std::filesystem
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(plugin_arch INTERFACE stdc++fs)
endif()

# ── CMake helpers for building plugins ───────────────────────────

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/PluginArch.cmake)

# ── Subdirectories ───────────────────────────────────────────────

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    if(PLUGIN_ARCH_BUILD_EXAMPLES)
        add_subdirectory(examples)
    endif()
    if(PLUGIN_ARCH_BUILD_TESTS)
        add_subdirectory(tests)
    endif()
endif()

# ── Install ──────────────────────────────────────────────────────

if(PLUGIN_ARCH_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    # Install headers
    install(DIRECTORY plugin_arch/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/plugin_arch
        FILES_MATCHING PATTERN "*.hpp"
    )

    # Install the INTERFACE target
    install(TARGETS plugin_arch
        EXPORT plugin_arch-targets
    )

    # Export target file
    install(EXPORT plugin_arch-targets
        FILE plugin_arch-targets.cmake
        NAMESPACE plugin_arch::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/plugin_arch
    )

    # Package config file
    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/plugin_arch-config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/plugin_arch-config.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/plugin_arch
    )

    # Version file
    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/plugin_arch-config-version.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/plugin_arch-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/plugin_arch-config-version.cmake
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/PluginArch.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/plugin_arch
    )
endif()
