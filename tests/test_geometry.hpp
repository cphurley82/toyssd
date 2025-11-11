// Copyright toyssd contributors
// SPDX-License-Identifier: MIT
//
// File: test_geometry.hpp
// Brief: Helpers for constructing common NandGeometry configurations in tests.

#pragma once

#include "toyssd/geometry.hpp"

namespace toyssd::test {

// Returns a geometry with caller-provided dimensions. Defaults match the small
// configurations most tests rely on so individual test cases only override the
// fields they care about.
inline NandGeometry MakeGeometry(uint32_t dies = 1, uint32_t blocks_per_die = 1,
                                 uint32_t pages_per_block = 1,
                                 uint32_t page_size_bytes = 4096,
                                 uint32_t oob_size_bytes = 256) {
  NandGeometry geometry;
  geometry.dies = dies;
  geometry.blocks_per_die = blocks_per_die;
  geometry.pages_per_block = pages_per_block;
  geometry.page_size_bytes = page_size_bytes;
  geometry.oob_size_bytes = oob_size_bytes;
  return geometry;
}

}  // namespace toyssd::test
