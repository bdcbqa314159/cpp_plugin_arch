add_executable(libB_host main.cpp)

target_link_libraries(libB_host PRIVATE plugin_arch)

target_include_directories(libB_host PRIVATE
    ${CMAKE_SOURCE_DIR}/examples/libB_example/interfaces
)
