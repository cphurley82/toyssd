## Unit tests for toyssd core
#
# We keep tests in a dedicated binary to isolate dependencies and speed up
# discovery. Listing sources explicitly (instead of globbing) makes intent
# clear in reviews and prevents accidental inclusion of WIP files.

set(TEST_SOURCES
    test_main.cc
    host_test.cc
    controller_test.cc
    nand_test.cc
)

add_executable(toyssd_core_tests ${TEST_SOURCES})
target_link_libraries(
    toyssd_core_tests
    PRIVATE
        toyssd_core
        GTest::gtest_main
)

# Integrate with CTest via GoogleTest’s discovery. This avoids maintaining
# a test list by hand and works well with IDEs and CI. Tests are registered
# at configure time by scanning the test binary.
include(GoogleTest)
gtest_discover_tests(toyssd_core_tests)
