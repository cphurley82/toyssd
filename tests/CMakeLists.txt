# unit_tests — GoogleTest-based unit test binary
# Contains FTL and NAND unit tests. The test list is auto-discovered by
# gtest_discover_tests() below, which queries the binary at CTest time.
add_executable(unit_tests
  test_main.cpp
  test_ftl.cpp
  test_nand.cpp
)

# Allow tests to include simulator headers directly
# TODO: We should probably be including everything relative to the source root
target_include_directories(unit_tests PRIVATE ${CMAKE_SOURCE_DIR}/sim)

# Link the core simulator, SystemC, and GoogleTest main
target_link_libraries(unit_tests PRIVATE simlib systemc gtest_main)

include(GoogleTest)
# Register individual tests dynamically (avoids needing to keep add_test in
# sync; works with filtering via ctest -R <regex>)
gtest_discover_tests(unit_tests)
