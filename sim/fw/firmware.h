// Copyright Chris Hurley
#pragma once
#include <memory>

#include "sim/config.h"
#include "sim/fw/ftl.h"
#include "sim/host/host_interface.h"
#include "sim/nand/nand_interface.h"
#include "sim/util/compat.h"
#include "systemc"

struct Firmware : sc_core::sc_module {
  sc_core::sc_fifo_in<IORequest*> in;
  sc_core::sc_fifo_out<Completion> out;
  INandInterface* nand_if{nullptr};
  FTL ftl;

  SC_CTOR(Firmware)
      : ftl(get_simulator_config().nand_blocks_per_die,
            get_simulator_config().nand_pages_per_block) {
    SC_THREAD(run);
  }
  void set_nand(INandInterface* n) { nand_if = n; }
  void run();
  sc_core::sc_time ctrl_overhead() const {
    const auto& cfg = get_simulator_config();
    return sc_core::sc_time(cfg.controller_overhead_us, sc_core::SC_US);
  }
};
