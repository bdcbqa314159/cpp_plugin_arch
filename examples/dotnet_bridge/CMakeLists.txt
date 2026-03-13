if(NOT MSVC OR DEFINED ENV{CI})
    message(STATUS "Skipping dotnet_bridge example (requires MSVC with .NET Framework, not available in CI)")
    return()
endif()

add_subdirectory(managed_assembly)
add_subdirectory(bridge)
add_subdirectory(host)
