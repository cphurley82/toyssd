// Copyright toyssd contributors
// SPDX-License-Identifier: MIT
//
// File: host.hpp
// Brief: SystemC "Host" model that initiates NVMe-like transactions toward the
//        controller using a tlm_utils::simple_initiator_socket.
//
// Responsibilities:
//  - Validate host-side I/O parameters (alignment, sizes).
//  - Construct tlm_generic_payload with NvmeCommandExtension metadata.
//  - Perform blocking transport and surface errors as exceptions to tests.
//
// Notes:
//  - The sector size is configurable at construction; defaults to 4096 bytes.

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

  // Submits a write beginning at the specified LBA. Data length must be a
  // multiple of the sector size. Throws std::invalid_argument on parameter
  // errors and std::runtime_error if the controller reports failure.
  void submit_write(uint64_t lba, const std::vector<uint8_t>& data,
                    DataPattern pattern);

  // Submits a read beginning at the specified LBA reading length_blocks logical
  // blocks. Returns a buffer sized length_blocks * sector_size_bytes_. Throws
  // std::invalid_argument if length_blocks == 0 and std::runtime_error on
  // controller failure.
  std::vector<uint8_t> submit_read(uint64_t lba, uint16_t length_blocks);

 private:
  uint32_t sector_size_bytes_;
  uint16_t next_command_id_{1};

  // Helper that performs the actual blocking transport and common error checks.
  void send_payload(tlm::tlm_generic_payload& payload,
                    NvmeCommandExtension& extension, sc_core::sc_time& delay);
};

}  // namespace toyssd
