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

include(GoogleTest)
gtest_discover_tests(toyssd_core_tests)
