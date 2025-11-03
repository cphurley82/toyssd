# unit_tests — GoogleTest-based unit test binary
# WHAT: Contains FTL and NAND unit tests.
# WHY: Tests are discovered at CTest time with gtest_discover_tests() so we
#   don't have to keep add_test() entries in sync and to support ctest -R.
add_executable(unit_tests test_main.cc test_ftl.cc)
target_compile_features(unit_tests PRIVATE cxx_std_20)
toyssd_apply_werror(unit_tests)
toyssd_apply_clang_tidy(unit_tests)

# Allow tests to include headers relative to the source root (e.g., "sim/...")
target_include_directories(unit_tests PRIVATE ${CMAKE_SOURCE_DIR})

# Link the core simulator, SystemC, and GoogleTest main
target_link_libraries(unit_tests PRIVATE simlib systemc gtest_main)
# Build and install SystemC before compiling tests that include <systemc>.
add_dependencies(unit_tests systemc_ext)

# Standardize test binary output to bin/ to simplify RPATH
set_target_properties(unit_tests PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/bin"
)

if(APPLE)
  # With standardized output dirs, tests live in bin/ and libs in ../lib
  set(_unit_tests_rpath "@loader_path/../lib")
else()
  set(_unit_tests_rpath "$ORIGIN/../lib")
endif()
list(APPEND _unit_tests_rpath "${SYSTEMC_PREFIX}/lib")
set_target_properties(unit_tests PROPERTIES BUILD_RPATH "${_unit_tests_rpath}")
# WHY: Keep build-tree lookup simple: local lib dir and absolute SystemC path.

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
