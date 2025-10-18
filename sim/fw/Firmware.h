#pragma once
#include <memory>

#include "../host/HostInterface.h"
#include "../nand/NandInterface.h"
#include "FTL.h"
#include "systemc"

struct Firmware : sc_core::sc_module {
  sc_core::sc_fifo_in<IORequest*> in;
  sc_core::sc_fifo_out<Completion> out;
  INandInterface* nand_if{nullptr};
  FTL ftl;

  SC_CTOR(Firmware) : ftl(1024, 256) { SC_THREAD(run); }
  void set_nand(INandInterface* n) { nand_if = n; }
  void run();
  sc_core::sc_time ctrl_overhead() const {
    return sc_core::sc_time(5, sc_core::SC_US);
  }
};
