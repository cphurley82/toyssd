// Copyright toyssd contributors
// SPDX-License-Identifier: MIT
//
// File: geometry.hpp
// Brief: Defines NAND geometry parameters and helper methods used throughout
//        the simulator to compute capacities and addressing.
//
// Rationale:
//  - Kept as a simple aggregate to pass across C++ and Python boundaries.
//  - Helper methods are constexpr to enable compile-time evaluation where
//    feasible and avoid hidden dynamic costs.

#pragma once

#include <cstdint>

namespace toyssd {

struct NandGeometry {
  // Number of independent dies (chips) available to the controller.
  uint32_t dies{1};
  // Blocks per die.
  uint32_t blocks_per_die{128};
  // Pages per block.
  uint32_t pages_per_block{64};
  // Payload bytes per page (OOB not included).
  uint32_t page_size_bytes{4096};
  // Out-of-band area size per page; reserved for future ECC/metadata modeling.
  uint32_t oob_size_bytes{256};

  // Total physical block count across all dies.
  [[nodiscard("use the returned value")]] constexpr uint64_t total_blocks()
      const {
    return static_cast<uint64_t>(dies) * blocks_per_die;
  }

  // Total physical page count across all dies.
  [[nodiscard("use the returned value")]] constexpr uint64_t total_pages()
      const {
    return total_blocks() * pages_per_block;
  }

  // Total user payload capacity (bytes), excluding OOB.
  [[nodiscard]] constexpr uint64_t total_capacity_bytes() const {
    return total_pages() * page_size_bytes;
  }

  // Number of logical blocks that fit in one page for a given sector size.
  [[nodiscard("use the returned value")]] constexpr uint32_t
  logical_blocks_per_page(uint32_t sector_size_bytes = 4096) const {
    return page_size_bytes / sector_size_bytes;
  }
};

}  // namespace toyssd
