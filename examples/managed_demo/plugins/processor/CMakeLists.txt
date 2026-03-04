add_library(managed_processor SHARED processor.cpp)

target_link_libraries(managed_processor PRIVATE plugin_arch)

target_include_directories(managed_processor PRIVATE
    ${CMAKE_SOURCE_DIR}/examples/managed_demo/interfaces
)

set_target_properties(managed_processor PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
