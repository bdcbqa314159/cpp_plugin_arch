add_executable(libA_host main.cpp)

target_link_libraries(libA_host PRIVATE plugin_arch)

target_include_directories(libA_host PRIVATE
    ${CMAKE_SOURCE_DIR}/examples/libA_example/interfaces
)
