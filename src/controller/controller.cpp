// Copyright toyssd contributors
// SPDX-License-Identifier: MIT
//
// File: controller.cpp
// Brief: Implements the Controller model bridging host NVMe-like commands to
//        NAND operations using blocking transport. Performs basic validation
//        and LBA -> Nand mapping.

#include <algorithm>
#include <memory>
#include <stdexcept>

#include "toyssd/controller.hpp"

namespace toyssd {

namespace {
constexpr uint32_t kLogicalBlockSizeBytes = 4096;
}

Controller::Controller(const sc_core::sc_module_name& name,
                       NandGeometry geometry)
    : sc_core::sc_module(name),
      host_socket_("host_socket_"),
      nand_socket_("nand_socket_"),
      geometry_(geometry) {
  if (geometry_.dies == 0U) {
    throw std::invalid_argument("Controller requires at least one die");
  }
  if (geometry_.blocks_per_die == 0U) {
    throw std::invalid_argument("Controller requires blocks_per_die > 0");
  }
  if (geometry_.pages_per_block == 0U) {
    throw std::invalid_argument("Controller requires pages_per_block > 0");
  }
  if (geometry_.page_size_bytes == 0U) {
    throw std::invalid_argument("Controller requires page_size_bytes > 0");
  }

  logical_blocks_per_page_ =
      std::max(1U, geometry_.page_size_bytes / kLogicalBlockSizeBytes);
  host_socket_.register_b_transport(this, &Controller::b_transport);
}

void Controller::b_transport(tlm::tlm_generic_payload& payload,
                             sc_core::sc_time& delay) {
  // Expect an NvmeCommandExtension to accompany all host transactions.
  auto* command = payload.get_extension<NvmeCommandExtension>();
  if (command == nullptr) {
    payload.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
    return;
  }

  command->status = NvmeStatus::SUCCESS;

  switch (command->opcode) {
    case NvmeOpcode::WRITE:
      handle_write(payload, *command, delay);
      break;
    case NvmeOpcode::READ:
      handle_read(payload, *command, delay);
      break;
    default:
      command->status = NvmeStatus::INVALID_OPCODE;
      payload.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
      return;
  }
}

void Controller::handle_write(tlm::tlm_generic_payload& payload,
                              NvmeCommandExtension& command,
                              sc_core::sc_time& delay) {
  // Validate payload presence; we don't support zero-length writes.
  if (payload.get_data_ptr() == nullptr || payload.get_data_length() == 0) {
    command.status = NvmeStatus::INVALID_FIELD;
    payload.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
    return;
  }

  const auto phys = map_lba(command.lba);
  if (phys.die >= geometry_.dies) {
    command.status = NvmeStatus::CAPACITY_EXCEEDED;
    payload.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
    return;
  }

  tlm::tlm_generic_payload nand_payload;
  nand_payload.set_command(tlm::TLM_WRITE_COMMAND);
  nand_payload.set_address(0);
  nand_payload.set_data_ptr(payload.get_data_ptr());
  nand_payload.set_data_length(payload.get_data_length());
  nand_payload.set_streaming_width(payload.get_streaming_width());
  nand_payload.set_byte_enable_ptr(nullptr);
  nand_payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

  auto nand_ext = std::make_unique<NandCommandExtension>();
  nand_ext->command_type = NandCommandType::PROGRAM;
  nand_ext->die = static_cast<uint8_t>(phys.die);
  nand_ext->block = static_cast<uint16_t>(phys.block);
  nand_ext->page = static_cast<uint16_t>(phys.page);
  nand_ext->length_bytes = payload.get_data_length();
  nand_ext->pattern = command.pattern;
  nand_ext->pattern_seed = command.pattern_seed;
  nand_payload.set_extension(nand_ext.get());

  nand_socket_->b_transport(nand_payload, delay);
  if (nand_payload.get_response_status() != tlm::TLM_OK_RESPONSE) {
    command.status = NvmeStatus::INTERNAL_ERROR;
    payload.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
  } else {
    payload.set_response_status(tlm::TLM_OK_RESPONSE);
  }

  const auto* nand_result = nand_payload.get_extension<NandCommandExtension>();
  if (nand_result != nullptr && nand_result->status != NandStatus::SUCCESS) {
    command.status = NvmeStatus::WRITE_FAULT;
    payload.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
  }

  nand_payload.clear_extension<NandCommandExtension>();
  // unique_ptr scope cleanup frees the extension once detached.
}

void Controller::handle_read(tlm::tlm_generic_payload& payload,
                             NvmeCommandExtension& command,
                             sc_core::sc_time& delay) {
  // Validate read buffer presence and size.
  if (payload.get_data_ptr() == nullptr || payload.get_data_length() == 0) {
    command.status = NvmeStatus::INVALID_FIELD;
    payload.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
    return;
  }

  const auto phys = map_lba(command.lba);
  if (phys.die >= geometry_.dies) {
    command.status = NvmeStatus::CAPACITY_EXCEEDED;
    payload.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
    return;
  }

  tlm::tlm_generic_payload nand_payload;
  nand_payload.set_command(tlm::TLM_READ_COMMAND);
  nand_payload.set_address(0);
  nand_payload.set_data_ptr(payload.get_data_ptr());
  nand_payload.set_data_length(payload.get_data_length());
  nand_payload.set_streaming_width(payload.get_streaming_width());
  nand_payload.set_byte_enable_ptr(nullptr);
  nand_payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

  auto nand_ext = std::make_unique<NandCommandExtension>();
  nand_ext->command_type = NandCommandType::READ;
  nand_ext->die = static_cast<uint8_t>(phys.die);
  nand_ext->block = static_cast<uint16_t>(phys.block);
  nand_ext->page = static_cast<uint16_t>(phys.page);
  nand_ext->length_bytes = payload.get_data_length();
  nand_ext->pattern = command.pattern;
  nand_ext->pattern_seed = command.pattern_seed;
  nand_payload.set_extension(nand_ext.get());

  nand_socket_->b_transport(nand_payload, delay);
  if (nand_payload.get_response_status() != tlm::TLM_OK_RESPONSE) {
    command.status = NvmeStatus::INTERNAL_ERROR;
    payload.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
  } else {
    payload.set_response_status(tlm::TLM_OK_RESPONSE);
  }

  const auto* nand_result = nand_payload.get_extension<NandCommandExtension>();
  if (nand_result != nullptr && nand_result->status != NandStatus::SUCCESS) {
    command.status = NvmeStatus::INTERNAL_ERROR;
    payload.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
  }

  nand_payload.clear_extension<NandCommandExtension>();
  // unique_ptr scope cleanup frees the extension once detached.
}

NandPhysicalAddress Controller::map_lba(uint64_t lba) const {
  // Compute a linear page index, then derive die/block/page using geometry.
  const uint64_t page_index = lba / logical_blocks_per_page_;
  const uint64_t pages_per_die =
      static_cast<uint64_t>(geometry_.blocks_per_die) *
      geometry_.pages_per_block;

  NandPhysicalAddress address;
  address.die = static_cast<uint32_t>(page_index / pages_per_die);
  const uint64_t die_local_page = page_index % pages_per_die;
  address.block =
      static_cast<uint32_t>(die_local_page / geometry_.pages_per_block);
  address.page =
      static_cast<uint32_t>(die_local_page % geometry_.pages_per_block);
  return address;
}

}  // namespace toyssd
