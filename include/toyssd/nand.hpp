// Copyright toyssd contributors
// SPDX-License-Identifier: MIT
//
// File: nand.hpp
// Brief: SystemC model of a NAND array. Supports PROGRAM,
//        READ, and ERASE commands via blocking transport on a target socket.
//
// Simplifications:
//  - Storage modeled as an in-memory unordered_map keyed by die/block/page.
//
// Concurrency:
//  - All accesses occur inside a single SystemC simulation thread using
//    blocking transport; no external synchronization required.

#pragma once

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "toyssd/extensions.hpp"
#include "toyssd/geometry.hpp"

namespace toyssd {

class Nand : public sc_core::sc_module {
 public:
  tlm_utils::simple_target_socket<Nand> target_socket;

  Nand(const sc_core::sc_module_name& name, NandGeometry geometry);

  // Main blocking transport entry point. Dispatches based on the attached
  // NandCommandExtension's command_type. Populates response_status and status
  // fields accordingly.
  void b_transport(tlm::tlm_generic_payload& payload, sc_core::sc_time& delay);

 private:
  NandGeometry geometry_;
  uint32_t page_size_bytes_{0};
  std::unordered_map<uint64_t, std::vector<uint8_t>> storage_;

  // Constructs a unique key for (die, block, page) for unordered_map storage.
  // TODO(cphurley): Input could be something like NandPhysicalAddress.
  static uint64_t make_page_key(uint32_t die, uint32_t block, uint32_t page);

  // Handles page program operations: copies payload data into storage_.
  // TODO(cphurley): Passing payload and command seems redundant; could pass
  // just command.
  void handle_program(tlm::tlm_generic_payload& payload,
                      NandCommandExtension& command);

  // Handles page read operations: copies stored data into payload buffer.
  // TODO(cphurley): Passing payload and command seems redundant; could pass
  // just command.
  void handle_read(tlm::tlm_generic_payload& payload,
                   NandCommandExtension& command);

  // Handles block erase: removes all pages within the specified block from
  // storage_.
  // TODO(cphurley): Why does this only take command while others take payload
  // too?
  void handle_erase(NandCommandExtension& command);
};

}  // namespace toyssd
