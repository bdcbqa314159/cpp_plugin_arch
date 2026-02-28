add_library(math_functions SHARED math_functions.cpp)

target_link_libraries(math_functions PRIVATE plugin_arch)

set_target_properties(math_functions PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
