add_library(managed_logger SHARED logger.cpp)

target_link_libraries(managed_logger PRIVATE plugin_arch)

target_include_directories(managed_logger PRIVATE
    ${CMAKE_SOURCE_DIR}/examples/managed_demo/interfaces
)

set_target_properties(managed_logger PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
