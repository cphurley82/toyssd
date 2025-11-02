// Copyright toyssd contributors
#pragma once

#include <cstdint>

/// Simulator configuration parameters.
///
/// This lightweight structure holds configuration values that are shared across
/// all simulator components. Values mirror the JSON schema consumed by
/// ssdsim_init() and control NAND geometry, timing, and controller behavior.
///
/// Default values provide a minimal configuration suitable for quick testing.
struct SimulatorConfig {
  // NAND geometry parameters
  uint32_t nand_dies{2};                ///< Number of NAND dies in the system
  uint32_t nand_blocks_per_die{256};    ///< Blocks per die
  uint32_t nand_pages_per_block{128};   ///< Pages per block
  uint32_t nand_page_size_bytes{4096};  ///< Page size in bytes

  // NAND timing parameters (microseconds)
  double nand_t_read_us{50.0};     ///< Page read latency
  double nand_t_prog_us{600.0};    ///< Page program latency
  double nand_t_erase_us{3000.0};  ///< Block erase latency

  // Controller parameters
  double controller_overhead_us{
      5.0};  ///< Additional controller processing overhead

  // Random number seed for deterministic simulation
  uint32_t rng_seed{1337};  ///< RNG seed for reproducibility
};

/// Returns the active simulator configuration.
///
/// This accesses a global singleton that must be initialized via
/// set_simulator_config() before constructing any SystemC modules.
///
/// @return Reference to the current configuration
const SimulatorConfig& get_simulator_config();

/// Sets the active simulator configuration.
///
/// This overwrites the global configuration singleton and should be called
/// during initialization before any SystemC modules are constructed. Changing
/// the configuration after module construction is not supported.
///
/// @param cfg The new configuration to apply
void set_simulator_config(const SimulatorConfig& cfg);
