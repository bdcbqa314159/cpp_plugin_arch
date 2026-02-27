# Builds the "vendor" artifact — liblibB_vendor.dylib/.so/.dll
# In real life you would NOT have this. You'd just have the binary.

add_library(libB_vendor SHARED libB.cpp)

target_include_directories(libB_vendor PUBLIC
    ${CMAKE_SOURCE_DIR}/examples/libB_example/prebuilt/include
)

# No post-build copy — the artifact lands in CMAKE_LIBRARY_OUTPUT_DIRECTORY
# alongside all other shared libraries (build/bin/).
