// Copyright toyssd contributors
// SPDX-License-Identifier: MIT
//
// File: test_main.cc
// Brief: GoogleTest entry point. SystemC sc_main stub provided for potential
//        future integration tests requiring kernel startup.

#include <gtest/gtest.h>
#include <systemc>

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

int sc_main(int /*argc*/, char* /*argv*/[]) { return 0; }
