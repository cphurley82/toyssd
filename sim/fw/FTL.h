#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>

struct PhysicalPage {
  uint32_t die{0}, block{0}, page{0};
};

class FTL {
 public:
  FTL(uint32_t blocks_per_die, uint32_t pages_per_block)
      : blocks(blocks_per_die), pages_per_block(pages_per_block) {}

  PhysicalPage map_write(uint64_t lba) {
    PhysicalPage p{0, next_block, next_page};
    l2p[(size_t)lba] = p;
    advance();
    return p;
  }
  PhysicalPage map_read(uint64_t lba) const {
    auto it = l2p.find((size_t)lba);
    if (it != l2p.end()) return it->second;
    return PhysicalPage{0, 0, 0};
  }

 private:
  void advance() {
    if (++next_page >= pages_per_block) {
      next_page = 0;
      next_block = (next_block + 1) % blocks;
    }
  }

  uint32_t blocks{1024};
  uint32_t pages_per_block{256};
  uint32_t next_block{0};
  uint32_t next_page{0};
  std::unordered_map<size_t, PhysicalPage> l2p;
};
