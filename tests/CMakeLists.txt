add_executable(unit_tests
  test_main.cpp
  test_ftl.cpp
  test_nand.cpp
)

target_include_directories(unit_tests PRIVATE ${CMAKE_SOURCE_DIR}/sim)
target_link_libraries(unit_tests PRIVATE simlib systemc gtest_main)

include(GoogleTest)
gtest_discover_tests(unit_tests)
