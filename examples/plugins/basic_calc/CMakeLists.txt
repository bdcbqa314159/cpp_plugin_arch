add_library(basic_calc SHARED basic_calc.cpp)

target_link_libraries(basic_calc PRIVATE plugin_arch)

target_include_directories(basic_calc PRIVATE
    ${CMAKE_SOURCE_DIR}/examples/interfaces
)

# Ensure symbol visibility for the factory functions
set_target_properties(basic_calc PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
