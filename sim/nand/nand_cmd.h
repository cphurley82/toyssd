// Copyright Chris Hurley
#pragma once

// Shim header to expose NandCmd without changing existing includes.
// NandCmd is currently defined in nand_model.h; include it here so tests and
// clients can depend on this path: "sim/nand/nand_cmd.h".
#include "sim/nand/nand_model.h"
