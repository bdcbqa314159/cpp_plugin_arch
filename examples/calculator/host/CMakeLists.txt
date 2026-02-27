add_executable(host main.cpp)

target_link_libraries(host PRIVATE plugin_arch)

target_include_directories(host PRIVATE
    ${CMAKE_SOURCE_DIR}/examples/calculator/interfaces
)
