# C++/CLI bridge DLL — compiled with /clr to host the .NET runtime.
# References ManagedMath.dll via #using (compile-time type safety, JIT optimized).

add_library(dotnet_bridge SHARED dotnet_bridge.cpp)

target_link_libraries(dotnet_bridge PRIVATE plugin_arch)

# Enable Common Language Runtime support.
# The empty string "" means "pure /clr" (not /clr:pure or /clr:safe).
set_target_properties(dotnet_bridge PROPERTIES
    COMMON_LANGUAGE_RUNTIME ""
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# /clr is incompatible with /EHsc (synchronous exceptions only) —
# must use /EHa (asynchronous, includes SEH). Also strip /RTC1
# (runtime checks) which conflicts with /clr.
target_compile_options(dotnet_bridge PRIVATE /EHa)
string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")

# Tell the compiler where to find ManagedMath.dll for #using resolution.
target_compile_options(dotnet_bridge PRIVATE "/AI${CMAKE_BINARY_DIR}/bin")

# Ensure ManagedMath.dll is built before the bridge.
add_dependencies(dotnet_bridge ManagedMath)
