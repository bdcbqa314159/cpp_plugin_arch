# Build the vendor artifact first (simulates receiving a pre-built .dylib)
add_subdirectory(libB_source)

# Then the adapter plugin and host
add_subdirectory(adapter)
add_subdirectory(host)
