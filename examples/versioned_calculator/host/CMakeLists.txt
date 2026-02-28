add_executable(versioned_calculator_host main.cpp)

target_link_libraries(versioned_calculator_host PRIVATE plugin_arch)

target_include_directories(versioned_calculator_host PRIVATE
    ${CMAKE_SOURCE_DIR}/examples/calculator/interfaces
    ${CMAKE_SOURCE_DIR}/examples/versioned_calculator/interfaces
)
