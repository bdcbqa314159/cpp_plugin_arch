# PluginArch.cmake — helper functions for building plugins and plugin hosts.
#
# Usage:
#   include(cmake/PluginArch.cmake)
#
#   add_plugin(my_plugin
#       SOURCES my_plugin.cpp
#       INTERFACE_DIRS ${CMAKE_SOURCE_DIR}/examples/my_demo/interfaces
#   )
#
#   add_plugin_host(my_host
#       SOURCES main.cpp
#       INTERFACE_DIRS ${CMAKE_SOURCE_DIR}/examples/my_demo/interfaces
#   )

# add_plugin(name SOURCES src... [INTERFACE_DIRS dir...])
# Creates a shared library target with:
#   - Hidden symbol visibility (only EXPORTED symbols are visible)
#   - Linked against plugin_arch interface library
#   - Include directories for the specified interface dirs
function(add_plugin TARGET_NAME)
    cmake_parse_arguments(PLUGIN "" "" "SOURCES;INTERFACE_DIRS" ${ARGN})

    if(NOT PLUGIN_SOURCES)
        message(FATAL_ERROR "add_plugin(${TARGET_NAME}): SOURCES is required")
    endif()

    add_library(${TARGET_NAME} SHARED ${PLUGIN_SOURCES})

    target_link_libraries(${TARGET_NAME} PRIVATE plugin_arch)

    if(PLUGIN_INTERFACE_DIRS)
        target_include_directories(${TARGET_NAME} PRIVATE ${PLUGIN_INTERFACE_DIRS})
    endif()

    set_target_properties(${TARGET_NAME} PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
endfunction()

# add_plugin_host(name SOURCES src... [INTERFACE_DIRS dir...])
# Creates an executable target with:
#   - Linked against plugin_arch interface library
#   - Include directories for the specified interface dirs
function(add_plugin_host TARGET_NAME)
    cmake_parse_arguments(HOST "" "" "SOURCES;INTERFACE_DIRS" ${ARGN})

    if(NOT HOST_SOURCES)
        message(FATAL_ERROR "add_plugin_host(${TARGET_NAME}): SOURCES is required")
    endif()

    add_executable(${TARGET_NAME} ${HOST_SOURCES})

    target_link_libraries(${TARGET_NAME} PRIVATE plugin_arch)

    if(HOST_INTERFACE_DIRS)
        target_include_directories(${TARGET_NAME} PRIVATE ${HOST_INTERFACE_DIRS})
    endif()
endfunction()
