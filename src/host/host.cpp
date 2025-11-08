// Copyright toyssd contributors
// SPDX-License-Identifier: MIT
//
// File: host.cpp
// Brief: Implements the Host SystemC model that issues NVMe-like transactions
//        to the controller over a blocking-transport TLM socket.

#include "toyssd/host.hpp"

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace toyssd {

// Submits a write to the controller with parameter validation and error
// translation to C++ exceptions suitable for tests.
void Host::submit_write(uint64_t lba, const std::vector<uint8_t>& data,
                        DataPattern pattern) {
  if (data.empty()) {
    throw std::invalid_argument("write data must not be empty");
  }
  if (data.size() % sector_size_bytes_ != 0U) {
    throw std::invalid_argument("write length must align to sector size");
  }

  tlm::tlm_generic_payload payload;
  payload.set_command(tlm::TLM_WRITE_COMMAND);
  payload.set_address(0);
  payload.set_data_ptr(const_cast<uint8_t*>(data.data()));
  payload.set_data_length(data.size());
  payload.set_streaming_width(data.size());
  payload.set_byte_enable_ptr(nullptr);
  payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

  NvmeCommandExtension* ext = new NvmeCommandExtension();
  ext->opcode = NvmeOpcode::WRITE;
  ext->lba = lba;
  ext->length = static_cast<uint16_t>(data.size() / sector_size_bytes_);
  ext->pattern = pattern;
  ext->command_id = next_command_id_++;
  payload.set_extension(ext);

  sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
  send_payload(payload, *ext, delay);
  NvmeStatus status = NvmeStatus::SUCCESS;
  if (auto* status_ext = payload.get_extension<NvmeCommandExtension>()) {
    status = status_ext->status;
  }
  NvmeCommandExtension* released = nullptr;
  payload.release_extension(released);
  delete released;
  if (status != NvmeStatus::SUCCESS) {
    throw std::runtime_error("NVMe write failed");
  }
}

// Submits a read to the controller and returns the received buffer. Performs
// basic argument validation and converts controller failures to exceptions.
std::vector<uint8_t> Host::submit_read(uint64_t lba, uint16_t length_blocks) {
  if (length_blocks == 0) {
    throw std::invalid_argument("length_blocks must be positive");
  }
  const auto total_bytes =
      static_cast<size_t>(length_blocks) * sector_size_bytes_;
  std::vector<uint8_t> buffer(total_bytes);

  tlm::tlm_generic_payload payload;
  payload.set_command(tlm::TLM_READ_COMMAND);
  payload.set_address(0);
  payload.set_data_ptr(buffer.data());
  payload.set_data_length(buffer.size());
  payload.set_streaming_width(buffer.size());
  payload.set_byte_enable_ptr(nullptr);
  payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

  NvmeCommandExtension* ext = new NvmeCommandExtension();
  ext->opcode = NvmeOpcode::READ;
  ext->lba = lba;
  ext->length = length_blocks;
  ext->command_id = next_command_id_++;
  payload.set_extension(ext);

  sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
  send_payload(payload, *ext, delay);

  NvmeStatus status = NvmeStatus::SUCCESS;
  if (auto* status_ext = payload.get_extension<NvmeCommandExtension>()) {
    status = status_ext->status;
  }
  NvmeCommandExtension* released = nullptr;
  payload.release_extension(released);
  delete released;
  if (status != NvmeStatus::SUCCESS) {
    throw std::runtime_error("NVMe read failed");
  }

  return buffer;
}

// Common transport helper used by read/write paths. Blocks until completion
// and checks TLM response status; waits on delay if a non-zero delay is set to
// preserve deterministic sequencing.
void Host::send_payload(tlm::tlm_generic_payload& payload,
                        NvmeCommandExtension& extension,
                        sc_core::sc_time& delay) {
  (void)extension;
  nvme_socket->b_transport(payload, delay);
  if (payload.get_response_status() != tlm::TLM_OK_RESPONSE) {
    throw std::runtime_error("TLM transaction failed");
  }
  if (delay > sc_core::SC_ZERO_TIME) {
    sc_core::wait(delay);
  }
}

}  // namespace toyssd
