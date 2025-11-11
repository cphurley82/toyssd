// Copyright toyssd contributors
// SPDX-License-Identifier: MIT
//
// File: test_geometry.hpp
// Brief: Helpers for constructing common NandGeometry configurations in tests.

#pragma once

#include <cstdint>

#include "toyssd/geometry.hpp"

namespace toyssd::test {

inline constexpr uint32_t kDefaultBlocksPerDie = 1;
inline constexpr uint32_t kDefaultPagesPerBlock = 1;
inline constexpr uint32_t kDefaultPageSizeBytes = 4096;
inline constexpr uint32_t kDefaultOobSizeBytes = 256;

// Returns a geometry with caller-provided dimensions. Defaults match the small
// configurations most tests rely on so individual test cases only override the
// fields they care about.
inline NandGeometry MakeGeometry(
    uint32_t dies = 1, uint32_t blocks_per_die = kDefaultBlocksPerDie,
    uint32_t pages_per_block = kDefaultPagesPerBlock,
    uint32_t page_size_bytes = kDefaultPageSizeBytes,
    uint32_t oob_size_bytes = kDefaultOobSizeBytes) {
  NandGeometry geometry;
  geometry.dies = dies;
  geometry.blocks_per_die = blocks_per_die;
  geometry.pages_per_block = pages_per_block;
  geometry.page_size_bytes = page_size_bytes;
  geometry.oob_size_bytes = oob_size_bytes;
  return geometry;
}

}  // namespace toyssd::test
