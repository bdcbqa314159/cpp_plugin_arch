# Intentionally broken plugins for testing the diagnostic tool.

add_library(crash_in_constructor SHARED crash_in_constructor.cpp)
add_library(missing_symbols SHARED missing_symbols.cpp)
add_library(crash_on_load SHARED crash_on_load.cpp)

# Hide symbols by default (realistic scenario)
foreach(target crash_in_constructor missing_symbols crash_on_load)
    set_target_properties(${target} PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
endforeach()
