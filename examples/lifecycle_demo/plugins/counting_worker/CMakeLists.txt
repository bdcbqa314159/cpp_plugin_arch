add_library(counting_worker SHARED counting_worker.cpp)

target_link_libraries(counting_worker PRIVATE plugin_arch)

target_include_directories(counting_worker PRIVATE
    ${CMAKE_SOURCE_DIR}/examples/lifecycle_demo/interfaces
)

set_target_properties(counting_worker PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
