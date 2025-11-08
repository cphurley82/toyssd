// Copyright toyssd contributors
// SPDX-License-Identifier: MIT
//
// File: systemc_hello.cpp
// Brief: Minimal SystemC smoke test used to validate the toolchain and build
//        integration. It instantiates a module that prints a greeting then
//        stops the kernel.

#include <iostream>

#include <systemc>

// clang-format off
SC_MODULE(HelloModule) {  // Simple module defining a single SC_THREAD.
  SC_CTOR(HelloModule) { SC_THREAD(run); }

  void run() {
    std::cout << "toyssd SystemC hello" << std::endl;
    sc_core::wait(sc_core::SC_ZERO_TIME);  // Allow delta-cycle scheduling.
    sc_core::sc_stop();                    // Terminate the kernel.
  }
};
// clang-format on

int sc_main(int argc, char* argv[]) {
  (void)argc;
  (void)argv;
  HelloModule module("hello");
  sc_core::sc_start();
  return 0;
}
