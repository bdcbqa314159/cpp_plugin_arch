add_library(managed_aggregator SHARED aggregator.cpp)

target_link_libraries(managed_aggregator PRIVATE plugin_arch)

target_include_directories(managed_aggregator PRIVATE
    ${CMAKE_SOURCE_DIR}/examples/managed_demo/interfaces
)

set_target_properties(managed_aggregator PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
