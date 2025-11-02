// Copyright Chris Hurley
#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>

/// Represents a physical page address in the NAND flash array.
struct PhysicalPage {
  uint32_t die{0};
  uint32_t block{0};
  uint32_t page{0};
};

/// Flash Translation Layer (FTL) implementing simple sequential write mapping.
///
/// This is a simplified FTL that maps logical block addresses (LBAs) to
/// physical pages using a sequential write pattern. It maintains a logical-to-
/// physical (L2P) mapping table and allocates pages sequentially across blocks.
///
/// Limitations:
/// - No garbage collection
/// - No wear leveling
/// - Single die only
/// - Sequential allocation pattern
class FTL {
 public:
  /// Constructs an FTL instance.
  ///
  /// @param blocks_per_die Number of blocks available per die
  /// @param pages_per_block Number of pages in each block
  FTL(uint32_t blocks_per_die, uint32_t pages_per_block)
      : blocks(blocks_per_die), pages_per_block(pages_per_block) {}

  /// Maps a write operation to a physical page.
  ///
  /// Allocates the next available physical page sequentially and updates
  /// the L2P mapping table.
  ///
  /// @param lba Logical block address to write
  /// @return Physical page address where data should be written
  PhysicalPage map_write(uint64_t lba) {
    PhysicalPage p{0, next_block, next_page};
    l2p[static_cast<size_t>(lba)] = p;
    advance();
    return p;
  }

  /// Maps a read operation to a physical page.
  ///
  /// @param lba Logical block address to read
  /// @return Physical page address if mapped, otherwise a zero-initialized page
  PhysicalPage map_read(uint64_t lba) const {
    auto it = l2p.find(static_cast<size_t>(lba));
    if (it != l2p.end()) return it->second;
    return PhysicalPage{0, 0, 0};
  }

 private:
  /// Advances the write pointer to the next physical page.
  ///
  /// Wraps to the next block when a block is full, and wraps around to
  /// block 0 when all blocks are exhausted (simple circular allocation).
  void advance() {
    if (++next_page >= pages_per_block) {
      next_page = 0;
      next_block = (next_block + 1) % blocks;
    }
  }

  uint32_t blocks;           ///< Total number of blocks per die
  uint32_t pages_per_block;  ///< Number of pages per block
  uint32_t next_block{0};    ///< Next block to allocate
  uint32_t next_page{0};     ///< Next page within the current block
  std::unordered_map<size_t, PhysicalPage>
      l2p;  ///< Logical-to-physical mapping
};
