// Copyright toyssd contributors

#pragma once

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <cstdint>
#include <vector>

#include "toyssd/extensions.hpp"

namespace toyssd {

class Host : public sc_core::sc_module {
 public:
  tlm_utils::simple_initiator_socket<Host> nvme_socket;

  explicit Host(const sc_core::sc_module_name& name,
                uint32_t sector_size_bytes = 4096)
      : sc_core::sc_module(name),
        nvme_socket("nvme_socket"),
        sector_size_bytes_(sector_size_bytes) {}

  void submit_write(uint64_t lba, const std::vector<uint8_t>& data,
                    DataPattern pattern);
  std::vector<uint8_t> submit_read(uint64_t lba, uint16_t length_blocks);

 private:
  uint32_t sector_size_bytes_;
  uint16_t next_command_id_{1};

  void send_payload(tlm::tlm_generic_payload& payload,
                    NvmeCommandExtension& extension, sc_core::sc_time& delay);
};

}  // namespace toyssd
