// Copyright toyssd contributors

#pragma once

#include <cstdint>
#include <tlm>  // NOLINT(build/include_order)

namespace toyssd {

enum class NvmeOpcode : uint8_t {
  READ = 0x02,
  WRITE = 0x01,
  FLUSH = 0x00,
};

enum class NvmeStatus : uint8_t {
  SUCCESS = 0x00,
  INVALID_OPCODE = 0x01,
  INVALID_FIELD = 0x02,
  CAPACITY_EXCEEDED = 0x08,
  INTERNAL_ERROR = 0x06,
  WRITE_FAULT = 0x80,
};

enum class DataPattern : uint8_t {
  SEQUENTIAL_COUNTER = 0,
  ZEROS = 1,
  ONES = 2,
};

struct NvmeCommandExtension : public tlm::tlm_extension<NvmeCommandExtension> {
  uint16_t command_id{0};
  NvmeOpcode opcode{NvmeOpcode::READ};
  uint32_t namespace_id{1};
  uint64_t lba{0};
  uint16_t length{1};
  DataPattern pattern{DataPattern::SEQUENTIAL_COUNTER};
  uint32_t pattern_seed{0};
  NvmeStatus status{NvmeStatus::SUCCESS};

  [[nodiscard]] tlm_extension_base* clone() const override;
  void copy_from(const tlm_extension_base& ext) override;
};

enum class NandCommandType : uint8_t {
  READ = 0x00,
  PROGRAM = 0x80,
  ERASE = 0x60,
};

enum class NandStatus : uint8_t {
  SUCCESS = 0x00,
  FAIL = 0x01,
  DATA_MISMATCH = 0x02,
};

struct NandPhysicalAddress {
  uint32_t die{0};
  uint32_t block{0};
  uint32_t page{0};
};

struct NandCommandExtension : public tlm::tlm_extension<NandCommandExtension> {
  NandCommandType command_type{NandCommandType::READ};
  uint8_t channel{0};
  uint8_t die{0};
  uint16_t block{0};
  uint16_t page{0};
  DataPattern pattern{DataPattern::SEQUENTIAL_COUNTER};
  uint32_t pattern_seed{0};
  uint32_t length_bytes{0};
  NandStatus status{NandStatus::SUCCESS};

  [[nodiscard]] tlm_extension_base* clone() const override;
  void copy_from(const tlm_extension_base& ext) override;
};

}  // namespace toyssd
