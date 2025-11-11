// Copyright toyssd contributors
// SPDX-License-Identifier: MIT
//
// File: controller.hpp
// Brief: SystemC Controller model bridging NVMe-like host commands to a
//        NAND interface. Performs address mapping and forwards
//        tlm_generic_payloads with NandCommandExtension.
//
// Responsibilities:
//  - Translate host LBAs to (die, block, page) addresses using geometry.
//  - Enforce parameter validation and surface status via NvmeStatus.
//  - Use blocking transport for deterministic flow.

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
  tlm_utils::simple_target_socket<Controller> host_socket_;
  tlm_utils::simple_initiator_socket<Controller> nand_socket_;

  Controller(const sc_core::sc_module_name& name, NandGeometry geometry);

  // Entry point for host-originated transactions. Expects an attached
  // NvmeCommandExtension; sets response status and command.status.
  void b_transport(tlm::tlm_generic_payload& payload, sc_core::sc_time& delay);

 private:
  NandGeometry geometry_;
  uint32_t logical_blocks_per_page_{1};

  // Write fast-path: validates payload, maps LBA, forwards to NAND, and
  // translates NAND completion to NVMe-style status.
  void handle_write(tlm::tlm_generic_payload& payload,
                    NvmeCommandExtension& command, sc_core::sc_time& delay);

  // Read fast-path: validates payload, maps LBA, forwards to NAND, and
  // translates NAND completion to NVMe-style status.
  void handle_read(tlm::tlm_generic_payload& payload,
                   NvmeCommandExtension& command, sc_core::sc_time& delay);

  // Computes die/block/page from a linear LBA based on current geometry.
  [[nodiscard(
      "physical address required for NAND command")]] NandPhysicalAddress
  map_lba(uint64_t lba) const;
};

}  // namespace toyssd
