add_library(greeter SHARED greeter.cpp)

target_link_libraries(greeter PRIVATE plugin_arch)

target_include_directories(greeter PRIVATE
    ${CMAKE_SOURCE_DIR}/examples/hot_reload_demo/interfaces
)

set_target_properties(greeter PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
