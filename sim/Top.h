#pragma once
#include "fw/Firmware.h"
#include "host/HostInterface.h"
#include "nand/NandInterface.h"
#include "nand/NandInterfaceImpl.h"
#include "nand/NandModel.h"
#include "systemc"

struct Top : sc_core::sc_module {
  HostInterface host{"host"};
  Firmware fw{"fw"};
  NandModel nand{"nand"};
  NandInterfaceImpl nand_if{"nand_if"};

  SC_CTOR(Top) {
    // Connect firmware queues
    fw.in.bind(host.to_fw);
    fw.out.bind(host.from_fw);
    // Connect NAND interface to model
    nand_if.socket.bind(nand.socket);
    fw.set_nand(&nand_if);
  }
};

// Factory used by C API bootstrap
namespace ssdsim_internal {
  inline sc_core::sc_module* create_top(HostInterface * *host_out) {
    auto* t = new Top("top");
    if (host_out) *host_out = &t->host;
    return static_cast<sc_core::sc_module*>(t);
  }
}
