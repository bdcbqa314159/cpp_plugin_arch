# Builds the "vendor" artifact — liblibB_vendor.dylib/.so/.dll
# In real life you would NOT have this. You'd just have the binary.

add_library(libB_vendor SHARED libB.cpp)

target_include_directories(libB_vendor PUBLIC
    ${CMAKE_SOURCE_DIR}/examples/libB_example/prebuilt/include
)

# On MSVC, generate an import library even without __declspec(dllexport).
set_target_properties(libB_vendor PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
