// Copyright Chris Hurley
#pragma once
#include <cstdint>

namespace toyssd {
namespace constants {

// FIFO buffer sizes
constexpr uint32_t kDefaultFifoSize = 1024;

// NAND flash constants
constexpr uint8_t kErasedByteValue = 0xFF;

// Simulation timing
constexpr int kPollMaxIterations = 100;
constexpr int kPollStepMicroseconds = 10;

// Nanoseconds per second for time conversion
constexpr double kNanosecondsPerSecond = 1'000'000'000.0;

}  // namespace constants
}  // namespace toyssd
