add_executable(service_locator_demo main.cpp)

target_link_libraries(service_locator_demo PRIVATE plugin_arch)

target_include_directories(service_locator_demo PRIVATE
    ${CMAKE_SOURCE_DIR}/examples/service_locator_demo/interfaces
)
