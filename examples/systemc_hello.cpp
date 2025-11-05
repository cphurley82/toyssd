#include <iostream>

#include <systemc>

// clang-format off
SC_MODULE(HelloModule) {
  SC_CTOR(HelloModule) {
    SC_THREAD(run);
  }

  void run() {
    std::cout << "toyssd SystemC hello" << std::endl;
    sc_core::wait(sc_core::SC_ZERO_TIME);
    sc_core::sc_stop();
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
