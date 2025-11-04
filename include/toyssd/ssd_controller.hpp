// Copyright toyssd contributors

#pragma once

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <cstdint>

#include "toyssd/extensions.hpp"
#include "toyssd/geometry.hpp"

namespace toyssd {

class Controller : public sc_core::sc_module {
 public:
  tlm_utils::simple_target_socket<Controller> host_socket;
  tlm_utils::simple_initiator_socket<Controller> nand_socket;

  Controller(const sc_core::sc_module_name& name, NandGeometry geometry);

  void b_transport(tlm::tlm_generic_payload& payload, sc_core::sc_time& delay);

 private:
  NandGeometry geometry_;
  uint32_t logical_blocks_per_page_{1};

  void handle_write(tlm::tlm_generic_payload& payload,
                    NvmeCommandExtension& command, sc_core::sc_time& delay);

  void handle_read(tlm::tlm_generic_payload& payload,
                   NvmeCommandExtension& command, sc_core::sc_time& delay);

  [[nodiscard]] NandPhysicalAddress map_lba(uint64_t lba) const;
};

}  // namespace toyssd
