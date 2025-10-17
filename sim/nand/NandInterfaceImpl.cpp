#include "NandInterfaceImpl.h"
#include "../fw/FTL.h" // for PhysicalPage definition

sc_core::sc_time NandInterfaceImpl::read(const PhysicalPage& p, uint8_t* /*dst*/) {
        NandCmd cmd; cmd.op = NandCmd::Op::READ; cmd.block = p.block; cmd.page = p.page; cmd.die = p.die;
        sc_core::sc_time d = sc_core::SC_ZERO_TIME;
        // Shortcut call into model (non-standard). In a full impl, use TLM GP + extension.
        auto* tgt = dynamic_cast<NandModel*>(socket.get_base_port().get_interface());
        if (tgt) tgt->b_transport(cmd, d);
        return d;
}

sc_core::sc_time NandInterfaceImpl::program(const PhysicalPage& p, const uint8_t* /*src*/) {
        NandCmd cmd; cmd.op = NandCmd::Op::PROGRAM; cmd.block = p.block; cmd.page = p.page; cmd.die = p.die;
        sc_core::sc_time d = sc_core::SC_ZERO_TIME;
        auto* tgt = dynamic_cast<NandModel*>(socket.get_base_port().get_interface());
        if (tgt) tgt->b_transport(cmd, d);
        return d;
}

sc_core::sc_time NandInterfaceImpl::erase(uint32_t die, uint32_t block) {
        NandCmd cmd; cmd.op = NandCmd::Op::ERASE; cmd.block = block; cmd.page = 0; cmd.die = die;
        sc_core::sc_time d = sc_core::SC_ZERO_TIME;
        auto* tgt = dynamic_cast<NandModel*>(socket.get_base_port().get_interface());
        if (tgt) tgt->b_transport(cmd, d);
        return d;
}
