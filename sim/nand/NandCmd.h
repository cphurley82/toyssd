// Copyright Chris Hurley
#pragma once

// Shim header to expose NandCmd without changing existing includes.
// NandCmd is currently defined in NandModel.h; include it here so tests and
// clients can depend on this path: "sim/nand/NandCmd.h".
#include "sim/nand/NandModel.h"
