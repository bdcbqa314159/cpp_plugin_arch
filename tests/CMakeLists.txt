include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.2
)
FetchContent_MakeAvailable(Catch2)

add_executable(plugin_arch_tests
    test_plugin_error.cpp
    test_event_bus.cpp
    test_mock_plugin_loader.cpp
    test_topological_sort.cpp
    test_service_locator.cpp
)

target_link_libraries(plugin_arch_tests PRIVATE
    plugin_arch
    Catch2::Catch2WithMain
)

include(CTest)
include(Catch)
catch_discover_tests(plugin_arch_tests)
