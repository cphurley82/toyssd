// Copyright toyssd contributors
// SPDX-License-Identifier: MIT
//
// File: host_test.cc
// Brief: Tests host-side parameter validation for write alignment.

#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

#include "toyssd/host.hpp"

namespace toyssd::test {

TEST(HostTest, RejectsMisalignedWrite) {
    Host host("host");
    std::vector<uint8_t> data(100);  // Not a multiple of 4096 bytes
    EXPECT_THROW(host.submit_write(0, data, DataPattern::SEQUENTIAL_COUNTER), std::invalid_argument);
}

}  // namespace toyssd::test
