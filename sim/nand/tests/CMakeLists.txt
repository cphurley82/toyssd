# nand_tests — GoogleTest-based unit tests for the NAND subsystem
add_executable(nand_tests
  nand_test.cpp
  ${CMAKE_SOURCE_DIR}/tests/test_main.cpp # Provide sc_main that runs GoogleTest
)
target_compile_features(nand_tests PRIVATE cxx_std_20)

# Allow tests to include headers relative to the source root (e.g., "sim/...")
target_include_directories(nand_tests PRIVATE ${CMAKE_SOURCE_DIR})

# Link the core simulator, SystemC, and GoogleTest main
target_link_libraries(nand_tests PRIVATE simlib systemc gtest_main)

include(GoogleTest)

# Where to write GoogleTest XML results (one file per discovered test)
set(TOYSSD_GTEST_XML_DIR ${CMAKE_BINARY_DIR}/test-results/gtest)
file(MAKE_DIRECTORY ${TOYSSD_GTEST_XML_DIR})

gtest_discover_tests(nand_tests
  XML_OUTPUT_DIR ${TOYSSD_GTEST_XML_DIR}
  PROPERTIES ENVIRONMENT "GTEST_COLOR=1"
)
