#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "toyssd/nand.hpp"

namespace toyssd::test {

TEST(NandTest, EraseClearsStoredData) {
    NandGeometry geometry;
    geometry.dies = 1;
    geometry.blocks_per_die = 1;
    geometry.pages_per_block = 2;
    geometry.page_size_bytes = 4096;

    Nand nand("nand", geometry);

    std::vector<uint8_t> page(4096, 0x55);

    tlm::tlm_generic_payload program_payload;
    program_payload.set_command(tlm::TLM_WRITE_COMMAND);
    program_payload.set_data_ptr(page.data());
    program_payload.set_data_length(page.size());
    program_payload.set_streaming_width(page.size());
    program_payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    auto program_ext = std::make_unique<NandCommandExtension>();
    program_ext->command_type = NandCommandType::PROGRAM;
    program_ext->die = 0;
    program_ext->block = 0;
    program_ext->page = 0;
    program_ext->length_bytes = page.size();
    program_payload.set_extension(program_ext.get());
    program_ext.release();

    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    nand.b_transport(program_payload, delay);
    EXPECT_EQ(program_payload.get_response_status(), tlm::TLM_OK_RESPONSE);

    auto* program_after = program_payload.get_extension<NandCommandExtension>();
    ASSERT_NE(program_after, nullptr);
    EXPECT_EQ(program_after->status, NandStatus::SUCCESS);
    NandCommandExtension* released_program = nullptr;
    program_payload.release_extension(released_program);
    if (released_program != nullptr) {
      program_ext.reset(released_program);
    }

    // Issue erase command
    tlm::tlm_generic_payload erase_payload;
    erase_payload.set_command(tlm::TLM_IGNORE_COMMAND);
    erase_payload.set_data_ptr(nullptr);
    erase_payload.set_data_length(0);
    erase_payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    auto erase_ext = std::make_unique<NandCommandExtension>();
    erase_ext->command_type = NandCommandType::ERASE;
    erase_ext->die = 0;
    erase_ext->block = 0;
    erase_ext->page = 0;
    erase_payload.set_extension(erase_ext.get());
    erase_ext.release();

    nand.b_transport(erase_payload, delay);
    EXPECT_EQ(erase_payload.get_response_status(), tlm::TLM_OK_RESPONSE);
    auto* erase_after = erase_payload.get_extension<NandCommandExtension>();
    ASSERT_NE(erase_after, nullptr);
    EXPECT_EQ(erase_after->status, NandStatus::SUCCESS);
    NandCommandExtension* released_erase = nullptr;
    erase_payload.release_extension(released_erase);
    if (released_erase != nullptr) {
      erase_ext.reset(released_erase);
    }

    // Attempt read and expect failure since data was erased.
    std::vector<uint8_t> read_buffer(4096);
    tlm::tlm_generic_payload read_payload;
    read_payload.set_command(tlm::TLM_READ_COMMAND);
    read_payload.set_data_ptr(read_buffer.data());
    read_payload.set_data_length(read_buffer.size());
    read_payload.set_streaming_width(read_buffer.size());
    read_payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    auto read_ext = std::make_unique<NandCommandExtension>();
    read_ext->command_type = NandCommandType::READ;
    read_ext->die = 0;
    read_ext->block = 0;
    read_ext->page = 0;
    read_ext->length_bytes = read_buffer.size();
    read_payload.set_extension(read_ext.get());
    read_ext.release();

    nand.b_transport(read_payload, delay);
    EXPECT_EQ(read_payload.get_response_status(), tlm::TLM_GENERIC_ERROR_RESPONSE);
    auto* read_after = read_payload.get_extension<NandCommandExtension>();
    ASSERT_NE(read_after, nullptr);
    EXPECT_EQ(read_after->status, NandStatus::FAIL);
    NandCommandExtension* released_read = nullptr;
    read_payload.release_extension(released_read);
    if (released_read != nullptr) {
      read_ext.reset(released_read);
    }
}

}  // namespace toyssd::test
