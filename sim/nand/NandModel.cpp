#include "NandModel.h"
using namespace sc_core;

void NandModel::b_transport(tlm::tlm_generic_payload& /*gp*/, sc_time& delay) {
    delay += sc_time(100, SC_US);
}

void NandModel::b_transport(NandCmd& cmd, sc_time& delay) {
    switch (cmd.op) {
        case NandCmd::Op::READ:    delay += sc_time(50, SC_US); break;
        case NandCmd::Op::PROGRAM: delay += sc_time(600, SC_US); break;
        case NandCmd::Op::ERASE:   delay += sc_time(3, SC_MS); break;
    }
}
