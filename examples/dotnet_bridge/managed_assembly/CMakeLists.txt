# Build ManagedMath.dll from C# source using the .NET Framework csc.exe.
# Skips gracefully if csc.exe is not found.

set(_csc_search_paths
    "$ENV{WINDIR}/Microsoft.NET/Framework64/v4.0.30319"
    "$ENV{WINDIR}/Microsoft.NET/Framework/v4.0.30319"
    "$ENV{WINDIR}/Microsoft.NET/Framework64/v3.5"
    "$ENV{WINDIR}/Microsoft.NET/Framework/v3.5"
    "$ENV{WINDIR}/Microsoft.NET/Framework64/v2.0.50727"
    "$ENV{WINDIR}/Microsoft.NET/Framework/v2.0.50727"
)

find_program(CSC_EXECUTABLE csc PATHS ${_csc_search_paths})

if(NOT CSC_EXECUTABLE)
    message(WARNING "csc.exe not found — skipping ManagedMath assembly build. "
                    "The bridge DLL will not compile without it.")
    return()
endif()

message(STATUS "Found csc.exe: ${CSC_EXECUTABLE}")

set(_cs_source "${CMAKE_CURRENT_SOURCE_DIR}/ManagedMath.cs")
set(_output_dll "${CMAKE_BINARY_DIR}/bin/ManagedMath.dll")

add_custom_command(
    OUTPUT "${_output_dll}"
    COMMAND "${CSC_EXECUTABLE}"
            /target:library
            /out:"${_output_dll}"
            "${_cs_source}"
    DEPENDS "${_cs_source}"
    COMMENT "Compiling ManagedMath.cs -> ManagedMath.dll"
    VERBATIM
)

add_custom_target(ManagedMath ALL DEPENDS "${_output_dll}")
