// Copyright Chris Hurley
#include "sim/nand/NandModel.h"

void NandModel::b_transport(tlm::tlm_generic_payload& /*gp*/,
                            sc_core::sc_time& delay) {
  delay += sc_core::sc_time(100, sc_core::SC_US);
}

void NandModel::b_transport(NandCmd& cmd, sc_core::sc_time& delay) {
  switch (cmd.op) {
    case NandCmd::Op::READ:
      delay += sc_core::sc_time(50, sc_core::SC_US);
      break;
    case NandCmd::Op::PROGRAM:
      delay += sc_core::sc_time(600, sc_core::SC_US);
      break;
    case NandCmd::Op::ERASE:
      delay += sc_core::sc_time(3, sc_core::SC_MS);
      break;
  }
}
