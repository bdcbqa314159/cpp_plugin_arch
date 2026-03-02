if(NOT MSVC)
    message(STATUS "Skipping dotnet_bridge example (requires MSVC for /clr)")
    return()
endif()

add_subdirectory(managed_assembly)
add_subdirectory(bridge)
add_subdirectory(host)
