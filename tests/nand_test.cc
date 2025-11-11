// Copyright toyssd contributors
// SPDX-License-Identifier: MIT
//
// File: nand_test.cc
// Brief: Exercises NAND PROGRAM/READ/ERASE success paths plus every
//        TLM_GENERIC_ERROR_RESPONSE edge case exposed by the model.

#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "toyssd/nand.hpp"
#include "tests/test_geometry.hpp"

namespace toyssd::test {

namespace {

// Allocates and initializes a payload with the provided buffer metadata.
std::unique_ptr<tlm::tlm_generic_payload> MakePayload(tlm::tlm_command command,
                                                      uint8_t* data_ptr,
                                                      size_t data_length,
                                                      size_t streaming_width) {
  auto payload = std::make_unique<tlm::tlm_generic_payload>();
  payload->set_command(command);
  payload->set_data_ptr(data_ptr);
  payload->set_data_length(data_length);
  payload->set_streaming_width(streaming_width);
  payload->set_byte_enable_ptr(nullptr);
  payload->set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
  return payload;
}

// Allocates and initializes a payload with the provided buffer.
std::unique_ptr<tlm::tlm_generic_payload> MakePayload(
    tlm::tlm_command command, std::vector<uint8_t>& buffer) {
  return MakePayload(command, buffer.data(), buffer.size(), buffer.size());
}

// Allocates a NAND command extension with the relevant addressing metadata.
std::unique_ptr<NandCommandExtension> MakeExtension(NandCommandType type,
                                                    uint8_t die = 0,
                                                    uint16_t block = 0,
                                                    uint16_t page = 0,
                                                    size_t length_bytes = 0) {
  auto ext = std::make_unique<NandCommandExtension>();
  ext->command_type = type;
  ext->die = die;
  ext->block = block;
  ext->page = page;
  ext->length_bytes = static_cast<uint32_t>(length_bytes);
  return ext;
}

// Releases the extension currently attached to the payload.
void ReleaseExtension(tlm::tlm_generic_payload& payload) {
  NandCommandExtension* released = nullptr;
  payload.release_extension(released);
  delete released;
}

}  // namespace

// Verifies basic program → erase → read behavior, ensuring erased data cannot
// be retrieved and yields the correct error path.
TEST(NandTest, EraseClearsStoredData) {
  auto geometry =
      MakeGeometry(/*dies=*/1, /*blocks_per_die=*/1, /*pages_per_block=*/2);

  auto nand = Nand("nand", geometry);

  constexpr uint8_t kProgramPattern = 0x55;
  auto page = std::vector<uint8_t>(geometry.page_size_bytes, kProgramPattern);

  auto program_payload = MakePayload(tlm::TLM_WRITE_COMMAND, page);
  auto program_ext =
      MakeExtension(NandCommandType::PROGRAM, 0, 0, 0, page.size());
  program_payload->set_extension(program_ext.get());
  program_ext.release();

  auto delay = sc_core::SC_ZERO_TIME;
  nand.b_transport(*program_payload, delay);
  EXPECT_EQ(program_payload->get_response_status(), tlm::TLM_OK_RESPONSE);

  auto* program_after = program_payload->get_extension<NandCommandExtension>();
  ASSERT_NE(program_after, nullptr);
  EXPECT_EQ(program_after->status, NandStatus::SUCCESS);
  ReleaseExtension(*program_payload);

  // Issue erase command
  auto erase_payload = MakePayload(tlm::TLM_IGNORE_COMMAND, nullptr, 0, 0);
  auto erase_ext = MakeExtension(NandCommandType::ERASE);
  erase_payload->set_extension(erase_ext.get());
  erase_ext.release();

  nand.b_transport(*erase_payload, delay);
  EXPECT_EQ(erase_payload->get_response_status(), tlm::TLM_OK_RESPONSE);
  auto* erase_after = erase_payload->get_extension<NandCommandExtension>();
  ASSERT_NE(erase_after, nullptr);
  EXPECT_EQ(erase_after->status, NandStatus::SUCCESS);
  ReleaseExtension(*erase_payload);

  // Attempt read and expect failure since data was erased.
  auto read_buffer = std::vector<uint8_t>(geometry.page_size_bytes);
  auto read_payload = MakePayload(tlm::TLM_READ_COMMAND, read_buffer);
  auto read_ext =
      MakeExtension(NandCommandType::READ, 0, 0, 0, read_buffer.size());
  read_payload->set_extension(read_ext.get());
  read_ext.release();

  nand.b_transport(*read_payload, delay);
  EXPECT_EQ(read_payload->get_response_status(),
            tlm::TLM_GENERIC_ERROR_RESPONSE);
  auto* read_after = read_payload->get_extension<NandCommandExtension>();
  ASSERT_NE(read_after, nullptr);
  EXPECT_EQ(read_after->status, NandStatus::FAIL);
  ReleaseExtension(*read_payload);
}

// Missing command metadata should immediately return a generic error.
TEST(NandTest, MissingExtensionReturnsError) {
  auto geometry = MakeGeometry();
  auto nand = Nand("nand_missing_ext", geometry);

  constexpr uint8_t kMissingExtensionPattern = 0xAB;
  auto buffer =
      std::vector<uint8_t>(geometry.page_size_bytes, kMissingExtensionPattern);

  auto payload = MakePayload(tlm::TLM_WRITE_COMMAND, buffer);

  auto delay = sc_core::SC_ZERO_TIME;
  nand.b_transport(*payload, delay);
  EXPECT_EQ(payload->get_response_status(), tlm::TLM_GENERIC_ERROR_RESPONSE);
}

// Commands other than PROGRAM/READ/ERASE should be rejected.
TEST(NandTest, UnsupportedCommandTypeReturnsError) {
  auto geometry = MakeGeometry();
  auto nand = Nand("nand_unsupported_cmd", geometry);

  constexpr uint8_t kUnsupportedCommandPattern = 0x11;
  auto buffer = std::vector<uint8_t>(geometry.page_size_bytes,
                                     kUnsupportedCommandPattern);

  auto payload = MakePayload(tlm::TLM_IGNORE_COMMAND, buffer);

  constexpr uint8_t kUnsupportedCommandValue = 0xFF;
  auto ext =
      MakeExtension(static_cast<NandCommandType>(kUnsupportedCommandValue));
  payload->set_extension(ext.get());
  ext.release();

  auto delay = sc_core::SC_ZERO_TIME;
  nand.b_transport(*payload, delay);
  EXPECT_EQ(payload->get_response_status(), tlm::TLM_GENERIC_ERROR_RESPONSE);
  auto* after = payload->get_extension<NandCommandExtension>();
  ASSERT_NE(after, nullptr);
  EXPECT_EQ(after->status, NandStatus::FAIL);
  ReleaseExtension(*payload);
}

// PROGRAM must have a data buffer; ensure NULL data fails.
TEST(NandTest, ProgramWithoutDataBufferFails) {
  auto geometry = MakeGeometry();
  auto nand = Nand("nand_program_missing_data", geometry);

  auto payload =
      MakePayload(tlm::TLM_WRITE_COMMAND, nullptr, geometry.page_size_bytes,
                  geometry.page_size_bytes);

  auto ext = MakeExtension(NandCommandType::PROGRAM, 0, 0, 0,
                           geometry.page_size_bytes);
  payload->set_extension(ext.get());
  ext.release();

  auto delay = sc_core::SC_ZERO_TIME;
  nand.b_transport(*payload, delay);
  EXPECT_EQ(payload->get_response_status(), tlm::TLM_GENERIC_ERROR_RESPONSE);
  auto* after = payload->get_extension<NandCommandExtension>();
  ASSERT_NE(after, nullptr);
  EXPECT_EQ(after->status, NandStatus::FAIL);
  ReleaseExtension(*payload);
}

// READ must have a destination buffer; ensure NULL buffer fails.
TEST(NandTest, ReadWithoutDestinationBufferFails) {
  auto geometry = MakeGeometry();
  auto nand = Nand("nand_read_missing_buffer", geometry);

  auto payload =
      MakePayload(tlm::TLM_READ_COMMAND, nullptr, geometry.page_size_bytes,
                  geometry.page_size_bytes);

  auto ext = MakeExtension(NandCommandType::READ);
  payload->set_extension(ext.get());
  ext.release();

  auto delay = sc_core::SC_ZERO_TIME;
  nand.b_transport(*payload, delay);
  EXPECT_EQ(payload->get_response_status(), tlm::TLM_GENERIC_ERROR_RESPONSE);
  auto* after = payload->get_extension<NandCommandExtension>();
  ASSERT_NE(after, nullptr);
  EXPECT_EQ(after->status, NandStatus::FAIL);
  ReleaseExtension(*payload);
}

// READ against an empty page should return FAIL status + generic error.
TEST(NandTest, ReadMissingPageFails) {
  auto geometry = MakeGeometry();
  auto nand = Nand("nand_read_missing_page", geometry);

  auto buffer = std::vector<uint8_t>(geometry.page_size_bytes);

  auto payload = MakePayload(tlm::TLM_READ_COMMAND, buffer);

  auto ext = MakeExtension(NandCommandType::READ);
  payload->set_extension(ext.get());
  ext.release();

  auto delay = sc_core::SC_ZERO_TIME;
  nand.b_transport(*payload, delay);
  EXPECT_EQ(payload->get_response_status(), tlm::TLM_GENERIC_ERROR_RESPONSE);
  auto* after = payload->get_extension<NandCommandExtension>();
  ASSERT_NE(after, nullptr);
  EXPECT_EQ(after->status, NandStatus::FAIL);
  ReleaseExtension(*payload);
}

}  // namespace toyssd::test
