// Copyright toyssd contributors
#include "sim/Config.h"

namespace {
SimulatorConfig g_config{};
}  // namespace

const SimulatorConfig& get_simulator_config() { return g_config; }

void set_simulator_config(const SimulatorConfig& cfg) { g_config = cfg; }
