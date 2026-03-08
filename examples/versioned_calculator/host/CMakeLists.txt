add_plugin_host(versioned_calculator_host
    SOURCES main.cpp
    INTERFACE_DIRS
        ${CMAKE_SOURCE_DIR}/examples/calculator/interfaces
        ${CMAKE_SOURCE_DIR}/examples/versioned_calculator/interfaces
)
