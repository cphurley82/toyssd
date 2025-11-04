// Copyright toyssd contributors

#include <algorithm>

#include "toyssd/ssd_controller.hpp"

namespace toyssd {

namespace {
constexpr uint32_t kLogicalBlockSizeBytes = 4096;
}

Controller::Controller(const sc_core::sc_module_name& name,
                       NandGeometry geometry)
    : sc_core::sc_module(name),
      host_socket("host_socket"),
      nand_socket("nand_socket"),
      geometry_(geometry) {
  logical_blocks_per_page_ =
      std::max(1U, geometry_.page_size_bytes / kLogicalBlockSizeBytes);
  host_socket.register_b_transport(this, &Controller::b_transport);
}

void Controller::b_transport(tlm::tlm_generic_payload& payload,
                             sc_core::sc_time& delay) {
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

  auto* nand_ext = new NandCommandExtension();
  nand_ext->command_type = NandCommandType::PROGRAM;
  nand_ext->die = static_cast<uint8_t>(phys.die);
  nand_ext->block = static_cast<uint16_t>(phys.block);
  nand_ext->page = static_cast<uint16_t>(phys.page);
  nand_ext->length_bytes = payload.get_data_length();
  nand_ext->pattern = command.pattern;
  nand_ext->pattern_seed = command.pattern_seed;
  nand_payload.set_extension(nand_ext);

  nand_socket->b_transport(nand_payload, delay);
  if (nand_payload.get_response_status() != tlm::TLM_OK_RESPONSE) {
    command.status = NvmeStatus::INTERNAL_ERROR;
    payload.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
  } else {
    payload.set_response_status(tlm::TLM_OK_RESPONSE);
  }

  NandCommandExtension* released = nullptr;
  nand_payload.release_extension(released);
  if (released != nullptr) {
    if (released->status != NandStatus::SUCCESS) {
      command.status = NvmeStatus::WRITE_FAULT;
      payload.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
    }
    delete released;
  }
}

void Controller::handle_read(tlm::tlm_generic_payload& payload,
                             NvmeCommandExtension& command,
                             sc_core::sc_time& delay) {
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

  auto* nand_ext = new NandCommandExtension();
  nand_ext->command_type = NandCommandType::READ;
  nand_ext->die = static_cast<uint8_t>(phys.die);
  nand_ext->block = static_cast<uint16_t>(phys.block);
  nand_ext->page = static_cast<uint16_t>(phys.page);
  nand_ext->length_bytes = payload.get_data_length();
  nand_ext->pattern = command.pattern;
  nand_ext->pattern_seed = command.pattern_seed;
  nand_payload.set_extension(nand_ext);

  nand_socket->b_transport(nand_payload, delay);
  if (nand_payload.get_response_status() != tlm::TLM_OK_RESPONSE) {
    command.status = NvmeStatus::INTERNAL_ERROR;
    payload.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
  } else {
    payload.set_response_status(tlm::TLM_OK_RESPONSE);
  }

  NandCommandExtension* released = nullptr;
  nand_payload.release_extension(released);
  if (released != nullptr) {
    if (released->status != NandStatus::SUCCESS) {
      command.status = NvmeStatus::INTERNAL_ERROR;
      payload.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
    }
    delete released;
  }
}

NandPhysicalAddress Controller::map_lba(uint64_t lba) const {
  const uint64_t page_index = lba / logical_blocks_per_page_;
  const uint64_t pages_per_die =
      static_cast<uint64_t>(geometry_.blocks_per_die) *
      geometry_.pages_per_block;

  NandPhysicalAddress address;
  if (pages_per_die == 0U) {
    address.die = 0;
    address.block = 0;
    address.page = 0;
    return address;
  }
  address.die = static_cast<uint32_t>(page_index / pages_per_die);
  const uint64_t die_local_page = page_index % pages_per_die;
  address.block =
      static_cast<uint32_t>(die_local_page / geometry_.pages_per_block);
  address.page =
      static_cast<uint32_t>(die_local_page % geometry_.pages_per_block);
  return address;
}

}  // namespace toyssd
