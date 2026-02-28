add_executable(hot_reload_demo main.cpp)

target_link_libraries(hot_reload_demo PRIVATE plugin_arch)

target_include_directories(hot_reload_demo PRIVATE
    ${CMAKE_SOURCE_DIR}/examples/hot_reload_demo/interfaces
)
