# The adapter plugin — links against the pre-built libB artifact.
# It does NOT need libB's source code, only the header + .dylib.

add_library(libB_adapter SHARED libB_adapter.cpp)

target_link_libraries(libB_adapter
    PRIVATE plugin_arch
    PRIVATE libB_vendor  # in real life: link against the prebuilt .dylib directly
)

target_include_directories(libB_adapter PRIVATE
    ${CMAKE_SOURCE_DIR}/examples/libB_example/interfaces
    ${CMAKE_SOURCE_DIR}/examples/libB_example/prebuilt/include
)

set_target_properties(libB_adapter PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
