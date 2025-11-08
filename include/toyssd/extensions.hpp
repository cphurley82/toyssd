// Copyright toyssd contributors
// SPDX-License-Identifier: MIT
//
// File: extensions.hpp
// Brief: Defines lightweight NVMe-like and NAND-like TLM extension data
//        structures plus related enumerations used to move metadata along the
//        host -> controller -> NAND datapath.
//
// Design Notes:
//  - These are intentionally minimal subsets of their real protocols to keep
//    the simulator deterministic and approachable.
//  - All enums are strongly typed (enum class) to prevent implicit integral
//    conversions and accidental mixing of domains.
//  - TL;DR: nvme -> controller uses NvmeCommandExtension; controller -> nand
//    uses NandCommandExtension. Each extension travels with a
//    tlm_generic_payload via blocking transport only.
//
// Extension Lifetime:
//  - clone() and copy_from() are required by the TLM extension interface. They
//    perform deep copies so payload duplication (e.g. for retries) preserves
//    command metadata.
//
// Threading & Concurrency:
//  - All usage is currently within a single SystemC kernel context with
//    blocking transport; no additional synchronization is needed.

#pragma once

#include <cstdint>
#include <tlm>  // NOLINT(build/include_order)

namespace toyssd {

// NvmeOpcode enumerates the small subset of NVMe-like operations supported by
// the simulator. Only READ and WRITE are used today; FLUSH is a placeholder for
// future extension (e.g., force FTL metadata commit).
enum class NvmeOpcode : uint8_t {
  READ = 0x02,
  WRITE = 0x01,
  FLUSH = 0x00,
};

// NvmeStatus captures simplified completion status codes. They intentionally
// mirror a few canonical NVMe codes to make reasoning familiar while remaining
// compact.
enum class NvmeStatus : uint8_t {
  SUCCESS = 0x00,
  INVALID_OPCODE = 0x01,
  INVALID_FIELD = 0x02,
  CAPACITY_EXCEEDED = 0x08,
  INTERNAL_ERROR = 0x06,
  WRITE_FAULT = 0x80,
};

// DataPattern selects a synthetic data generation scheme for write operations.
// It lets tests (and later, workload generators) verify data integrity without
// transporting large host buffers.
enum class DataPattern : uint8_t {
  SEQUENTIAL_COUNTER =
      0,      // Sequential byte counter starting from pattern_seed.
  ZEROS = 1,  // All 0x00 bytes.
  ONES = 2,   // All 0xFF bytes.
};

// NvmeCommandExtension carries host-originated command metadata through the
// controller. Length is expressed in logical blocks (LBAs). The pattern fields
// allow write data synthesis; for reads they are echoed for potential integrity
// checking higher up.
struct NvmeCommandExtension : public tlm::tlm_extension<NvmeCommandExtension> {
  uint16_t command_id{0};  // Monotonically incrementing host-side ID.
  NvmeOpcode opcode{NvmeOpcode::READ};
  uint32_t namespace_id{1};  // Single namespace for now (future multi-NS).
  uint64_t lba{0};           // Starting logical block address.
  uint16_t length{1};        // Count of logical blocks.
  DataPattern pattern{DataPattern::SEQUENTIAL_COUNTER};
  uint32_t pattern_seed{0};  // Seed for pattern generation.
  NvmeStatus status{
      NvmeStatus::SUCCESS};  // Completion status populated by controller.

  // Returns a heap-allocated deep copy of this extension. Marked nodiscard so
  // callers either attach it to a payload (transferring ownership) or delete
  // it to avoid leaking. The string explains intent in build output.
  [[nodiscard(
      "returned pointer must be attached to payload or explicitly "
      "deleted")]] tlm_extension_base*
  clone() const override;

  void copy_from(const tlm_extension_base& ext) override;
};

// NandCommandType reflects the minimal ONFI-like command families we model.
// PROGRAM == write a page; ERASE removes all pages in the target block.
enum class NandCommandType : uint8_t {
  READ = 0x00,
  PROGRAM = 0x80,
  ERASE = 0x60,
};

// NandStatus surfaces simplified outcomes of NAND operations.
enum class NandStatus : uint8_t {
  SUCCESS = 0x00,
  FAIL = 0x01,
  DATA_MISMATCH = 0x02,  // Reserved for future pattern verification.
};

// NandPhysicalAddress decomposes a page into die, block, and page indices.
// It is produced by address mapping logic in the controller.
// TODO(cphurley): add additional dimensions (channel, plane, physical page) for
// future geometry modeling.
struct NandPhysicalAddress {
  uint32_t die{0};
  uint32_t block{0};
  uint32_t page{0};
};

// NandCommandExtension is consumed directly by the NAND model. The channel
// field is a placeholder for future multi-channel geometry expansion.
// TODO(cphurley): use NandPhysicalAddress instead of separate fields.
struct NandCommandExtension : public tlm::tlm_extension<NandCommandExtension> {
  NandCommandType command_type{NandCommandType::READ};
  uint8_t channel{0};  // Currently unused; kept for future modeling.
  uint8_t die{0};      // Target die index.
  uint16_t block{0};   // Target block index within die.
  uint16_t page{0};    // Target page index within block.
  DataPattern pattern{DataPattern::SEQUENTIAL_COUNTER};
  uint32_t pattern_seed{0};  // Echoed from host command for verification.
  uint32_t length_bytes{0};  // Expected data payload size in bytes.
  NandStatus status{
      NandStatus::SUCCESS};  // Updated by NAND model on completion.
  // Returns a heap-allocated deep copy; same ownership expectations as above.
  [[nodiscard(
      "returned pointer must be attached to payload or explicitly "
      "deleted")]] tlm_extension_base*
  clone() const override;
  void copy_from(const tlm_extension_base& ext) override;
};

}  // namespace toyssd
