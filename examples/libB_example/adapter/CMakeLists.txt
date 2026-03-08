# The adapter plugin — links against the pre-built libB artifact.
# It does NOT need libB's source code, only the header + .dylib.

add_plugin(libB_adapter
    SOURCES libB_adapter.cpp
    INTERFACE_DIRS
        ${CMAKE_SOURCE_DIR}/examples/libB_example/interfaces
        ${CMAKE_SOURCE_DIR}/examples/libB_example/prebuilt/include
    LINK_LIBS libB_vendor
)
