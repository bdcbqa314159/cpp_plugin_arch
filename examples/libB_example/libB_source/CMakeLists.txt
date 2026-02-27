# Builds the "vendor" artifact — liblibB_vendor.dylib/.so/.dll
# In real life you would NOT have this. You'd just have the binary.

add_library(libB_vendor SHARED libB.cpp)

target_include_directories(libB_vendor PUBLIC
    ${CMAKE_SOURCE_DIR}/examples/libB_example/prebuilt/include
)

# Copy the artifact into prebuilt/lib/ so the adapter can find it
add_custom_command(TARGET libB_vendor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:libB_vendor>
        ${CMAKE_SOURCE_DIR}/examples/libB_example/prebuilt/lib/$<TARGET_FILE_NAME:libB_vendor>
)
