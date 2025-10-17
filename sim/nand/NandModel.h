#pragma once
#include "systemc"
#include "tlm.h"
#include "tlm_utils/simple_target_socket.h"

struct NandCmd {
    enum class Op { READ, PROGRAM, ERASE } op;
    uint32_t die{0}, block{0}, page{0};
    uint8_t* data{nullptr};
};

struct NandModel : sc_core::sc_module {
    tlm_utils::simple_target_socket<NandModel> socket;

    SC_CTOR(NandModel) {
        socket.register_b_transport(this, &NandModel::b_transport);
    }

    void b_transport(tlm::tlm_generic_payload& gp, sc_core::sc_time& delay);
    // Overload for custom payload shortcut (not standard TLM; placeholder)
    void b_transport(NandCmd& cmd, sc_core::sc_time& delay);
};
