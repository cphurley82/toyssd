// Copyright toyssd contributors

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

  void b_transport(tlm::tlm_generic_payload& payload, sc_core::sc_time& delay);

 private:
  NandGeometry geometry_;
  uint32_t page_size_bytes_{0};
  std::unordered_map<uint64_t, std::vector<uint8_t>> storage_;

  static uint64_t make_page_key(uint32_t die, uint32_t block, uint32_t page);

  void handle_program(tlm::tlm_generic_payload& payload,
                      NandCommandExtension& command);

  void handle_read(tlm::tlm_generic_payload& payload,
                   NandCommandExtension& command);

  void handle_erase(NandCommandExtension& command);
};

}  // namespace toyssd
