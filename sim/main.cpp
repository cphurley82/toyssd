#include "systemc"
#include "Top.h"

using namespace sc_core;

int sc_main(int argc, char* argv[]) {
    Top top{"top"};
    sc_start(); // Run until no events (the host is driven by C API)
    return 0;
}
