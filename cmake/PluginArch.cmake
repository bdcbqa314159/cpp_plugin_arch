# PluginArch.cmake — helper functions for building plugins and plugin hosts.
#
# These functions encapsulate the boilerplate for creating plugin shared
# libraries and host executables that use the plugin_arch library.
#
# Usage:
#   find_package(plugin_arch REQUIRED)   # or include() directly
#
#   add_plugin(my_plugin
#       SOURCES my_plugin.cpp
#       INTERFACE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/interfaces
#       LINK_LIBS some_dependency
#   )
#
#   add_plugin_host(my_host
#       SOURCES main.cpp
#       INTERFACE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/interfaces
#       LINK_LIBS some_dependency
#   )

# add_plugin(name SOURCES src... [INTERFACE_DIRS dir...] [LINK_LIBS lib...])
#
# Creates a shared library target with:
#   - Hidden symbol visibility (only EXPORTED symbols are visible)
#   - Linked against plugin_arch::plugin_arch
#   - Include directories for the specified interface dirs
#   - Additional link libraries if specified
function(add_plugin TARGET_NAME)
    cmake_parse_arguments(PLUGIN "" "" "SOURCES;INTERFACE_DIRS;LINK_LIBS" ${ARGN})

    if(NOT PLUGIN_SOURCES)
        message(FATAL_ERROR "add_plugin(${TARGET_NAME}): SOURCES is required")
    endif()

    add_library(${TARGET_NAME} SHARED ${PLUGIN_SOURCES})

    if(TARGET plugin_arch::plugin_arch)
        target_link_libraries(${TARGET_NAME} PRIVATE plugin_arch::plugin_arch)
    else()
        target_link_libraries(${TARGET_NAME} PRIVATE plugin_arch)
    endif()

    if(PLUGIN_INTERFACE_DIRS)
        target_include_directories(${TARGET_NAME} PRIVATE ${PLUGIN_INTERFACE_DIRS})
    endif()

    if(PLUGIN_LINK_LIBS)
        target_link_libraries(${TARGET_NAME} PRIVATE ${PLUGIN_LINK_LIBS})
    endif()

    set_target_properties(${TARGET_NAME} PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
endfunction()

# add_plugin_host(name SOURCES src... [INTERFACE_DIRS dir...] [LINK_LIBS lib...])
#
# Creates an executable target with:
#   - Linked against plugin_arch::plugin_arch
#   - Include directories for the specified interface dirs
#   - Additional link libraries if specified
function(add_plugin_host TARGET_NAME)
    cmake_parse_arguments(HOST "" "" "SOURCES;INTERFACE_DIRS;LINK_LIBS" ${ARGN})

    if(NOT HOST_SOURCES)
        message(FATAL_ERROR "add_plugin_host(${TARGET_NAME}): SOURCES is required")
    endif()

    add_executable(${TARGET_NAME} ${HOST_SOURCES})

    if(TARGET plugin_arch::plugin_arch)
        target_link_libraries(${TARGET_NAME} PRIVATE plugin_arch::plugin_arch)
    else()
        target_link_libraries(${TARGET_NAME} PRIVATE plugin_arch)
    endif()

    if(HOST_INTERFACE_DIRS)
        target_include_directories(${TARGET_NAME} PRIVATE ${HOST_INTERFACE_DIRS})
    endif()

    if(HOST_LINK_LIBS)
        target_link_libraries(${TARGET_NAME} PRIVATE ${HOST_LINK_LIBS})
    endif()
endfunction()
