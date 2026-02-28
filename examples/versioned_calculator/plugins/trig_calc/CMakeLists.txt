add_library(trig_calc SHARED trig_calc.cpp)

target_link_libraries(trig_calc PRIVATE plugin_arch)

target_include_directories(trig_calc PRIVATE
    ${CMAKE_SOURCE_DIR}/examples/calculator/interfaces
    ${CMAKE_SOURCE_DIR}/examples/versioned_calculator/interfaces
)

set_target_properties(trig_calc PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
