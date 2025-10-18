#include <gtest/gtest.h>

#include "systemc"

// Provide sc_main so SystemC's library is satisfied at load time.
// The actual program entry point remains gtest_main's main().
int sc_main(int argc, char* argv[]) {
  // Optionally, run tests from sc_main if SystemC ever provides main.
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
