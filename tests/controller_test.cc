#include <gtest/gtest.h>

#include <numeric>
#include <stdexcept>
#include <vector>

#include "toyssd/host.hpp"
#include "toyssd/nand.hpp"
#include "toyssd/ssd_controller.hpp"

namespace toyssd::test {

TEST(ControllerTest, WriteReadRoundtrip) {
    NandGeometry geometry;
    geometry.dies = 1;
    geometry.blocks_per_die = 2;
    geometry.pages_per_block = 4;
    geometry.page_size_bytes = 4096;

    Host host("host");
    Controller controller("controller", geometry);
    Nand nand("nand", geometry);

    host.nvme_socket.bind(controller.host_socket);
    controller.nand_socket.bind(nand.target_socket);

    std::vector<uint8_t> pattern(4096);
    std::iota(pattern.begin(), pattern.end(), 0);

    host.submit_write(0, pattern, DataPattern::SEQUENTIAL_COUNTER);
    auto result = host.submit_read(0, 1);

    EXPECT_EQ(result, pattern);
}

TEST(ControllerTest, CapacityExceededTriggersError) {
    NandGeometry geometry;
    geometry.dies = 1;
    geometry.blocks_per_die = 1;
    geometry.pages_per_block = 1;
    geometry.page_size_bytes = 4096;

    Host host("host");
    Controller controller("controller", geometry);
    Nand nand("nand", geometry);

    host.nvme_socket.bind(controller.host_socket);
    controller.nand_socket.bind(nand.target_socket);

    std::vector<uint8_t> pattern(4096, 0xAA);

    EXPECT_THROW(
        host.submit_write(10, pattern, DataPattern::SEQUENTIAL_COUNTER),
        std::runtime_error
    );
}

}  // namespace toyssd::test
