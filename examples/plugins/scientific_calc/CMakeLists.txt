add_library(scientific_calc SHARED scientific_calc.cpp)

target_link_libraries(scientific_calc PRIVATE plugin_arch)

target_include_directories(scientific_calc PRIVATE
    ${CMAKE_SOURCE_DIR}/examples/interfaces
)

# Ensure symbol visibility for the factory functions
set_target_properties(scientific_calc PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
