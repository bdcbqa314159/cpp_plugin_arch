add_executable(managed_demo main.cpp)

target_link_libraries(managed_demo PRIVATE plugin_arch)

target_include_directories(managed_demo PRIVATE
    ${CMAKE_SOURCE_DIR}/examples/managed_demo/interfaces
)
