# unit_tests — GoogleTest-based unit test binary
# Contains FTL and NAND unit tests. The test list is auto-discovered by
# gtest_discover_tests() below, which queries the binary at CTest time.
add_executable(unit_tests
  test_main.cpp
  test_ftl.cpp
)
target_compile_features(unit_tests PRIVATE cxx_std_20)
toyssd_apply_werror(unit_tests)
toyssd_apply_clang_tidy(unit_tests)

# Allow tests to include headers relative to the source root (e.g., "sim/...")
target_include_directories(unit_tests PRIVATE ${CMAKE_SOURCE_DIR})

# Link the core simulator, SystemC, and GoogleTest main
target_link_libraries(unit_tests PRIVATE simlib systemc gtest_main)

if(APPLE)
  set(_unit_tests_rpath "@loader_path/../../_vendor/systemc/lib;@loader_path/../../lib;@loader_path/../lib")
else()
  set(_unit_tests_rpath "$ORIGIN/../../_vendor/systemc/lib;$ORIGIN/../../lib;$ORIGIN/../lib")
endif()
list(APPEND _unit_tests_rpath "${SYSTEMC_PREFIX}/lib")
set_target_properties(unit_tests PROPERTIES BUILD_RPATH "${_unit_tests_rpath}")

include(GoogleTest)
# Register individual tests dynamically (avoids needing to keep add_test in
# sync; works with filtering via ctest -R <regex>). Also have GoogleTest emit
# native XML reports so CI can consume per-test results.

# Where to write GoogleTest XML results (one file per discovered test)
set(TOYSSD_GTEST_XML_DIR ${CMAKE_BINARY_DIR}/test-results/gtest)
file(MAKE_DIRECTORY ${TOYSSD_GTEST_XML_DIR})

gtest_discover_tests(unit_tests
  XML_OUTPUT_DIR ${TOYSSD_GTEST_XML_DIR}
  # Optional niceties
  PROPERTIES ENVIRONMENT "GTEST_COLOR=1"
)
