add_executable(lifecycle_demo main.cpp)

target_link_libraries(lifecycle_demo PRIVATE plugin_arch)

target_include_directories(lifecycle_demo PRIVATE
    ${CMAKE_SOURCE_DIR}/examples/lifecycle_demo/interfaces
)
