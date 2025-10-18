#include <gtest/gtest.h>

#include "nand/NandModel.h"

TEST(NandModelTest, LatencyOrder) {
  NandModel nand("nand");
  sc_core::sc_time d = sc_core::SC_ZERO_TIME;
  NandCmd c{NandCmd::Op::READ, 0, 0, 0, nullptr};
  nand.b_transport(c, d);
  EXPECT_GE(d, sc_core::sc_time(1, sc_core::SC_US));
}
