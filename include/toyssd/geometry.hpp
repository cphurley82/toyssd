// Copyright toyssd contributors

#pragma once

#include <cstdint>

namespace toyssd {

struct NandGeometry {
  uint32_t dies{1};
  uint32_t blocks_per_die{128};
  uint32_t pages_per_block{64};
  uint32_t page_size_bytes{4096};
  uint32_t oob_size_bytes{256};

  [[nodiscard]] constexpr uint64_t total_blocks() const {
    return static_cast<uint64_t>(dies) * blocks_per_die;
  }

  [[nodiscard]] constexpr uint64_t total_pages() const {
    return total_blocks() * pages_per_block;
  }

  [[nodiscard]] constexpr uint64_t total_capacity_bytes() const {
    return total_pages() * page_size_bytes;
  }

  [[nodiscard]] constexpr uint32_t logical_blocks_per_page(
      uint32_t sector_size_bytes = 4096) const {
    return page_size_bytes / sector_size_bytes;
  }
};

}  // namespace toyssd
