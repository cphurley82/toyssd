// Copyright toyssd contributors

#include "toyssd/nand.hpp"

#include <algorithm>
#include <vector>

namespace toyssd {

namespace {
constexpr uint64_t kDieShiftBits = 40;
constexpr uint64_t kBlockShiftBits = 20;
constexpr uint64_t kFieldMask = 0xFFFFF;
}  // namespace

Nand::Nand(const sc_core::sc_module_name& name, NandGeometry geometry)
    : sc_core::sc_module(name),
      target_socket("target_socket"),
      geometry_(geometry),
      page_size_bytes_(geometry.page_size_bytes) {
  target_socket.register_b_transport(this, &Nand::b_transport);
}

void Nand::b_transport(tlm::tlm_generic_payload& payload,
                       sc_core::sc_time& delay) {
  (void)delay;
  auto* command = payload.get_extension<NandCommandExtension>();
  if (command == nullptr) {
    payload.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
    return;
  }

  switch (command->command_type) {
    case NandCommandType::PROGRAM:
      handle_program(payload, *command);
      break;
    case NandCommandType::READ:
      handle_read(payload, *command);
      break;
    case NandCommandType::ERASE:
      handle_erase(*command);
      payload.set_response_status(tlm::TLM_OK_RESPONSE);
      break;
    default:
      command->status = NandStatus::FAIL;
      payload.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
      break;
  }
}

uint64_t Nand::make_page_key(uint32_t die, uint32_t block, uint32_t page) {
  return (static_cast<uint64_t>(die) << kDieShiftBits) |
         (static_cast<uint64_t>(block) << kBlockShiftBits) |
         static_cast<uint64_t>(page);
}

void Nand::handle_program(tlm::tlm_generic_payload& payload,
                          NandCommandExtension& command) {
  if (payload.get_data_ptr() == nullptr || payload.get_data_length() == 0) {
    command.status = NandStatus::FAIL;
    payload.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
    return;
  }
  const auto key = make_page_key(command.die, command.block, command.page);
  auto& page = storage_[key];
  page.assign(payload.get_data_ptr(),
              payload.get_data_ptr() + payload.get_data_length());
  command.status = NandStatus::SUCCESS;
  payload.set_response_status(tlm::TLM_OK_RESPONSE);
}

void Nand::handle_read(tlm::tlm_generic_payload& payload,
                       NandCommandExtension& command) {
  if (payload.get_data_ptr() == nullptr || payload.get_data_length() == 0) {
    command.status = NandStatus::FAIL;
    payload.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
    return;
  }
  const auto key = make_page_key(command.die, command.block, command.page);
  const auto entry_iter = storage_.find(key);
  if (entry_iter == storage_.end()) {
    command.status = NandStatus::FAIL;
    payload.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
    return;
  }

  std::copy(entry_iter->second.begin(), entry_iter->second.end(),
            payload.get_data_ptr());
  command.status = NandStatus::SUCCESS;
  payload.set_response_status(tlm::TLM_OK_RESPONSE);
}

void Nand::handle_erase(NandCommandExtension& command) {
  std::vector<uint64_t> keys_to_remove;
  keys_to_remove.reserve(geometry_.pages_per_block);
  for (const auto& entry : storage_) {
    const uint64_t stored_key = entry.first;
    const uint32_t die =
        static_cast<uint32_t>((stored_key >> kDieShiftBits) & kFieldMask);
    const uint32_t block =
        static_cast<uint32_t>((stored_key >> kBlockShiftBits) & kFieldMask);
    if (die == command.die && block == command.block) {
      keys_to_remove.push_back(stored_key);
    }
  }
  for (const auto key : keys_to_remove) {
    storage_.erase(key);
  }
  command.status = NandStatus::SUCCESS;
}

}  // namespace toyssd
