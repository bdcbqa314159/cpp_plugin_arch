add_plugin(report_generator
    SOURCES report_generator.cpp
    INTERFACE_DIRS
        ${CMAKE_SOURCE_DIR}/examples/service_locator_demo/interfaces
        ${CMAKE_SOURCE_DIR}/examples/libA_example/interfaces
        ${CMAKE_SOURCE_DIR}/examples/libB_example/interfaces
)
