# nand_tests — GoogleTest-based unit tests for the NAND subsystem
# WHY: Keep NAND-focused tests close to implementation to simplify includes
#   and enable running just this suite via ctest -R.
add_executable(nand_tests
  nand_test.cc
  ${CMAKE_SOURCE_DIR}/tests/test_main.cc  # Provide sc_main that runs GoogleTest
)
target_compile_features(nand_tests PRIVATE cxx_std_20)
toyssd_apply_werror(nand_tests)
toyssd_apply_clang_tidy(nand_tests)

# Allow tests to include headers relative to the source root (e.g., "sim/...")
target_include_directories(nand_tests PRIVATE ${CMAKE_SOURCE_DIR})

# Link the core simulator, SystemC, and GoogleTest main
target_link_libraries(nand_tests PRIVATE simlib systemc gtest_main)
# Ensure SystemC is available before compiling tests that include <systemc>.
add_dependencies(nand_tests systemc_ext)
# Place the binary under bin/ so relative RPATH logic holds.
toyssd_configure_output_dirs(nand_tests)

if(APPLE)
  # With standardized output dirs, tests live in bin/ and libs in ../lib
  set(_nand_tests_rpath "@loader_path/../lib")
else()
  set(_nand_tests_rpath "$ORIGIN/../lib")
endif()
list(APPEND _nand_tests_rpath "${SYSTEMC_PREFIX}/lib")
set_target_properties(nand_tests PROPERTIES BUILD_RPATH "${_nand_tests_rpath}")
# WHY: Keep build-tree lookup simple: local lib dir and absolute SystemC path.

include(GoogleTest)

# Where to write GoogleTest XML results (one file per discovered test)
set(TOYSSD_GTEST_XML_DIR ${CMAKE_BINARY_DIR}/test-results/gtest)
file(MAKE_DIRECTORY ${TOYSSD_GTEST_XML_DIR})

gtest_discover_tests(nand_tests
  XML_OUTPUT_DIR ${TOYSSD_GTEST_XML_DIR}
  PROPERTIES ENVIRONMENT "GTEST_COLOR=1"
)
