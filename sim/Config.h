// Copyright toyssd contributors
#pragma once

#include <cstdint>

// Lightweight configuration object shared across simulator components.
// Values mirror the JSON schema consumed by ssdsim_init().
struct SimulatorConfig {
  uint32_t nand_dies{2};
  uint32_t nand_blocks_per_die{256};
  uint32_t nand_pages_per_block{128};
  uint32_t nand_page_size_bytes{4096};
  double nand_t_read_us{50.0};
  double nand_t_prog_us{600.0};
  double nand_t_erase_us{3000.0};
  double controller_overhead_us{5.0};
  uint32_t rng_seed{1337};
};

// Returns a singleton snapshot of the active simulator configuration.
const SimulatorConfig& get_simulator_config();

// Overwrites the active simulator configuration. Intended to be called during
// initialization before any SystemC modules are constructed.
void set_simulator_config(const SimulatorConfig& cfg);
