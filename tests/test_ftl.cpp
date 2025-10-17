#include <gtest/gtest.h>
#include "fw/FTL.h"

TEST(FTLTest, WriteThenReadSamePage) {
    FTL ftl(16, 64);
    auto p1 = ftl.map_write(100);
    auto p2 = ftl.map_read(100);
    EXPECT_EQ(p1.block, p2.block);
    EXPECT_EQ(p1.page, p2.page);
}
