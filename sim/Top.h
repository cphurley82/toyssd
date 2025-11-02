// Copyright Chris Hurley
#pragma once
#include "sim/fw/Firmware.h"
#include "sim/host/HostInterface.h"
#include "sim/nand/NandInterface.h"
#include "sim/nand/NandInterfaceImpl.h"
#include "sim/nand/NandModel.h"
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
    nand_if.attach(&nand);
    fw.set_nand(&nand_if);
  }
};

// Factory used by C API bootstrap
namespace ssdsim_internal {
inline sc_core::sc_module* create_top(HostInterface** host_out) {
  auto* t = new Top("top");
  if (host_out) *host_out = &t->host;
  return static_cast<sc_core::sc_module*>(t);
}
}  // namespace ssdsim_internal
