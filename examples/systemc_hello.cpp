// Copyright toyssd contributors
// SPDX-License-Identifier: MIT
//
// File: systemc_hello.cpp
// Brief: Minimal SystemC smoke test used to validate the toolchain and build
//        integration. It instantiates a module that prints a greeting then
//        stops the kernel.

#include <iostream>

#include <systemc>

// Prefer plain C++ class form over SC_MODULE/SC_CTOR macros.
class HelloModule : public sc_core::sc_module {
 public:
  SC_HAS_PROCESS(HelloModule);
  explicit HelloModule(const sc_core::sc_module_name& name)
      : sc_core::sc_module(name) {
    SC_THREAD(run);
  }

 private:
  void run() {
    std::cout << "toyssd SystemC hello" << std::endl;
    sc_core::wait(sc_core::SC_ZERO_TIME);  // Allow delta-cycle scheduling.
    sc_core::sc_stop();                    // Terminate the kernel.
  }
};

int sc_main(int argc, char* argv[]) {
  (void)argc;
  (void)argv;
  HelloModule module("hello");
  sc_core::sc_start();
  return 0;
}
