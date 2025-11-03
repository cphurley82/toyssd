// Copyright Chris Hurley
#include <systemc>

#include "sim/top.h"

int sc_main(int argc, char* argv[]) {
  Top top{"top"};
  sc_core::sc_start();  // Run until no events (the host is driven by C API)
  return 0;
}
