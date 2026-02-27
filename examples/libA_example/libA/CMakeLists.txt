add_library(libA SHARED libA.cpp)

target_link_libraries(libA PRIVATE plugin_arch)

target_include_directories(libA PRIVATE
    ${CMAKE_SOURCE_DIR}/examples/libA_example/interfaces
)

set_target_properties(libA PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
