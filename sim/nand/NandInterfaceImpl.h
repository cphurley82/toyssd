// Copyright Chris Hurley
#pragma once
#include <systemc>

#include "sim/nand/NandInterface.h"
#include "sim/nand/NandModel.h"
#include "sim/util/Compat.h"
#include "tlm_utils/simple_initiator_socket.h"

struct NandInterfaceImpl : sc_core::sc_module, INandInterface {
  tlm_utils::simple_initiator_socket<NandInterfaceImpl> socket;

  SC_CTOR(NandInterfaceImpl) {}

  void attach(NandModel* model);

  sc_core::sc_time read(const PhysicalPage& p, uint8_t* dst) override;
  sc_core::sc_time program(const PhysicalPage& p, const uint8_t* src) override;
  sc_core::sc_time erase(uint32_t die, uint32_t block) override;

 private:
  NandModel* model_{nullptr};
};
