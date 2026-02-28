add_library(report_generator SHARED report_generator.cpp)

target_link_libraries(report_generator PRIVATE plugin_arch)

target_include_directories(report_generator PRIVATE
    ${CMAKE_SOURCE_DIR}/examples/service_locator_demo/interfaces
    ${CMAKE_SOURCE_DIR}/examples/libA_example/interfaces
    ${CMAKE_SOURCE_DIR}/examples/libB_example/interfaces
)

set_target_properties(report_generator PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
