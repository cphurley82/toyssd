// Copyright toyssd contributors
// SPDX-License-Identifier: MIT
//
// File: controller_test.cc
// Brief: Tests host/controller/nand write-read roundtrip and capacity error
//        surfacing via exceptions.

#include <gtest/gtest.h>

#include <memory>
#include <numeric>
#include <stdexcept>
#include <vector>
#include <tlm>

#include "toyssd/host.hpp"
#include "toyssd/nand.hpp"
#include "toyssd/ssd_controller.hpp"

namespace toyssd::test {

namespace {

class StubNand : public sc_core::sc_module {
 public:
  tlm_utils::simple_target_socket<StubNand> target_socket_{"target_socket"};
  tlm::tlm_response_status next_response_status_{tlm::TLM_OK_RESPONSE};
  NandStatus completion_status_{NandStatus::SUCCESS};
  bool extension_seen_{false};
  NandStatus last_status_{NandStatus::SUCCESS};

  explicit StubNand(const sc_core::sc_module_name& name)
      : sc_core::sc_module(name) {
    target_socket_.register_b_transport(this, &StubNand::b_transport);
  }

 private:
  void b_transport(tlm::tlm_generic_payload& payload, sc_core::sc_time& delay) {
    extension_seen_ = false;
    payload.set_response_status(next_response_status_);
    auto* command = payload.get_extension<NandCommandExtension>();
    if (command != nullptr) {
      extension_seen_ = true;
      command->status = completion_status_;
      last_status_ = command->status;
    }
    delay = sc_core::SC_ZERO_TIME;
  }
};

struct ControllerHarness {
  Controller controller_;
  StubNand nand_;

  explicit ControllerHarness(const NandGeometry& geometry)
      : controller_(sc_core::sc_gen_unique_name("controller"), geometry),
        nand_(sc_core::sc_gen_unique_name("stub_nand")) {
    controller_.nand_socket.bind(nand_.target_socket_);
  }
};

NandGeometry MakeDefaultGeometry() {
  NandGeometry geometry;
  geometry.dies = 1;
  geometry.blocks_per_die = 1;
  geometry.pages_per_block = 1;
  geometry.page_size_bytes = 4096;
  return geometry;
}

NvmeCommandExtension* AttachCommand(tlm::tlm_generic_payload& payload,
                                    NvmeOpcode opcode, uint64_t lba = 0) {
  auto* command = new NvmeCommandExtension();
  command->opcode = opcode;
  command->lba = lba;
  command->length = 1;
  payload.set_extension(command);
  return command;
}

void ReleaseCommand(tlm::tlm_generic_payload& payload) {
  NvmeCommandExtension* released = nullptr;
  payload.release_extension(released);
  delete released;
}

void ConfigurePayload(tlm::tlm_generic_payload& payload,
                      tlm::tlm_command command,
                      uint8_t* data_ptr,
                      size_t data_length,
                      size_t streaming_width) {
  payload.set_command(command);
  payload.set_data_ptr(data_ptr);
  payload.set_data_length(data_length);
  payload.set_streaming_width(streaming_width);
  payload.set_byte_enable_ptr(nullptr);
  payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
}

void ConfigurePayload(tlm::tlm_generic_payload& payload,
                      tlm::tlm_command command,
                      std::vector<uint8_t>& buffer) {
  ConfigurePayload(
      payload, command, buffer.data(), buffer.size(), buffer.size()
  );
}

std::unique_ptr<tlm::tlm_generic_payload> MakePayload(
    tlm::tlm_command command, uint8_t* data_ptr, size_t data_length,
    size_t streaming_width
) {
  auto payload = std::make_unique<tlm::tlm_generic_payload>();
  ConfigurePayload(*payload, command, data_ptr, data_length, streaming_width);
  return payload;
}

std::unique_ptr<tlm::tlm_generic_payload> MakePayload(
    tlm::tlm_command command, std::vector<uint8_t>& buffer
) {
  return MakePayload(command, buffer.data(), buffer.size(), buffer.size());
}

}  // namespace

TEST(ControllerTest, WriteReadRoundtrip) {
    auto geometry = NandGeometry();
    geometry.dies = 1;
    geometry.blocks_per_die = 2;
    geometry.pages_per_block = 4;
    geometry.page_size_bytes = 4096;

    auto host = Host("host");
    auto controller = Controller("controller", geometry);
    auto nand = Nand("nand", geometry);

    host.nvme_socket.bind(controller.host_socket);
    controller.nand_socket.bind(nand.target_socket);

    auto pattern = std::vector<uint8_t>(4096);
    std::iota(pattern.begin(), pattern.end(), 0);

    host.submit_write(0, pattern, DataPattern::SEQUENTIAL_COUNTER);
    auto result = host.submit_read(0, 1);

    EXPECT_EQ(result, pattern);
}

TEST(ControllerTest, CapacityExceededTriggersError) {
    auto geometry = NandGeometry();
    geometry.dies = 1;
    geometry.blocks_per_die = 1;
    geometry.pages_per_block = 1;
    geometry.page_size_bytes = 4096;

    auto host = Host("host");
    auto controller = Controller("controller", geometry);
    auto nand = Nand("nand", geometry);

    host.nvme_socket.bind(controller.host_socket);
    controller.nand_socket.bind(nand.target_socket);

    auto pattern = std::vector<uint8_t>(4096, 0xAA);

    EXPECT_THROW(
        host.submit_write(10, pattern, DataPattern::SEQUENTIAL_COUNTER),
        std::runtime_error
    );
}

TEST(ControllerTest, RejectsGeometryWithZeroDies) {
    auto geometry = MakeDefaultGeometry();
    geometry.dies = 0;
    EXPECT_THROW(Controller("controller_invalid_dies", geometry), std::invalid_argument);
}

TEST(ControllerTest, RejectsGeometryWithZeroBlocksPerDie) {
    auto geometry = MakeDefaultGeometry();
    geometry.blocks_per_die = 0;
    EXPECT_THROW(Controller("controller_invalid_blocks", geometry), std::invalid_argument);
}

TEST(ControllerTest, RejectsGeometryWithZeroPagesPerBlock) {
    auto geometry = MakeDefaultGeometry();
    geometry.pages_per_block = 0;
    EXPECT_THROW(Controller("controller_invalid_pages", geometry), std::invalid_argument);
}

TEST(ControllerTest, RejectsGeometryWithZeroPageSize) {
    auto geometry = MakeDefaultGeometry();
    geometry.page_size_bytes = 0;
    EXPECT_THROW(Controller("controller_invalid_page_size", geometry), std::invalid_argument);
}

TEST(ControllerTest, MissingNvmeExtensionReturnsError) {
    auto harness = ControllerHarness(MakeDefaultGeometry());

    auto data = std::vector<uint8_t>(4096, 0xCD);
    auto payload = MakePayload(tlm::TLM_WRITE_COMMAND, data);

    auto delay = sc_core::SC_ZERO_TIME;
    harness.controller_.b_transport(*payload, delay);

    EXPECT_EQ(payload->get_response_status(), tlm::TLM_GENERIC_ERROR_RESPONSE);
}

TEST(ControllerTest, InvalidOpcodeSetsGenericErrorResponse) {
    auto harness = ControllerHarness(MakeDefaultGeometry());

    auto data = std::vector<uint8_t>(4096, 0xCD);
    auto payload = MakePayload(tlm::TLM_IGNORE_COMMAND, data);
    AttachCommand(*payload, NvmeOpcode::FLUSH);

    auto delay = sc_core::SC_ZERO_TIME;
    harness.controller_.b_transport(*payload, delay);

    auto* command = payload->get_extension<NvmeCommandExtension>();
    ASSERT_NE(command, nullptr);
    EXPECT_EQ(command->status, NvmeStatus::INVALID_OPCODE);
    EXPECT_EQ(payload->get_response_status(), tlm::TLM_GENERIC_ERROR_RESPONSE);
    ReleaseCommand(*payload);
}

TEST(ControllerTest, WriteMissingDataBufferTriggersInvalidField) {
    auto harness = ControllerHarness(MakeDefaultGeometry());

    auto payload = MakePayload(tlm::TLM_WRITE_COMMAND, nullptr, 4096, 4096);
    AttachCommand(*payload, NvmeOpcode::WRITE);

    auto delay = sc_core::SC_ZERO_TIME;
    harness.controller_.b_transport(*payload, delay);

    auto* command = payload->get_extension<NvmeCommandExtension>();
    ASSERT_NE(command, nullptr);
    EXPECT_EQ(command->status, NvmeStatus::INVALID_FIELD);
    EXPECT_EQ(payload->get_response_status(), tlm::TLM_GENERIC_ERROR_RESPONSE);
    ReleaseCommand(*payload);
}

TEST(ControllerTest, WriteZeroLengthTriggersInvalidField) {
    auto harness = ControllerHarness(MakeDefaultGeometry());

    auto data = std::vector<uint8_t>(4096, 0);
    auto payload =
        MakePayload(tlm::TLM_WRITE_COMMAND, data.data(), 0, data.size());
    AttachCommand(*payload, NvmeOpcode::WRITE);

    auto delay = sc_core::SC_ZERO_TIME;
    harness.controller_.b_transport(*payload, delay);

    auto* command = payload->get_extension<NvmeCommandExtension>();
    ASSERT_NE(command, nullptr);
    EXPECT_EQ(command->status, NvmeStatus::INVALID_FIELD);
    EXPECT_EQ(payload->get_response_status(), tlm::TLM_GENERIC_ERROR_RESPONSE);
    ReleaseCommand(*payload);
}

TEST(ControllerTest, WriteCapacityExceededSetsNvmeStatus) {
    auto harness = ControllerHarness(MakeDefaultGeometry());

    auto data = std::vector<uint8_t>(4096, 0xFF);
    auto payload = MakePayload(tlm::TLM_WRITE_COMMAND, data);
    auto* command = AttachCommand(*payload, NvmeOpcode::WRITE, /*lba=*/1);
    command->length = 1;

    auto delay = sc_core::SC_ZERO_TIME;
    harness.controller_.b_transport(*payload, delay);

    auto* command_after = payload->get_extension<NvmeCommandExtension>();
    ASSERT_NE(command_after, nullptr);
    EXPECT_EQ(command_after->status, NvmeStatus::CAPACITY_EXCEEDED);
    EXPECT_EQ(payload->get_response_status(), tlm::TLM_GENERIC_ERROR_RESPONSE);
    ReleaseCommand(*payload);
}

TEST(ControllerTest, WritePropagatesNandTransportErrors) {
    auto harness = ControllerHarness(MakeDefaultGeometry());
    harness.nand_.next_response_status_ = tlm::TLM_GENERIC_ERROR_RESPONSE;

    auto data = std::vector<uint8_t>(4096, 0x1);
    auto payload = MakePayload(tlm::TLM_WRITE_COMMAND, data);
    AttachCommand(*payload, NvmeOpcode::WRITE);

    auto delay = sc_core::SC_ZERO_TIME;
    harness.controller_.b_transport(*payload, delay);

    auto* command = payload->get_extension<NvmeCommandExtension>();
    ASSERT_NE(command, nullptr);
    EXPECT_EQ(command->status, NvmeStatus::INTERNAL_ERROR);
    EXPECT_EQ(payload->get_response_status(), tlm::TLM_GENERIC_ERROR_RESPONSE);
    ReleaseCommand(*payload);
}

TEST(ControllerTest, WriteTranslatesNandFailureToWriteFault) {
    auto harness = ControllerHarness(MakeDefaultGeometry());
    harness.nand_.completion_status_ = NandStatus::FAIL;

    auto data = std::vector<uint8_t>(4096, 0x1);
    auto payload = MakePayload(tlm::TLM_WRITE_COMMAND, data);
    AttachCommand(*payload, NvmeOpcode::WRITE);

    auto delay = sc_core::SC_ZERO_TIME;
    harness.controller_.b_transport(*payload, delay);

    EXPECT_TRUE(harness.nand_.extension_seen_);
    EXPECT_EQ(harness.nand_.last_status_, NandStatus::FAIL);
    auto* command = payload->get_extension<NvmeCommandExtension>();
    ASSERT_NE(command, nullptr);
    EXPECT_EQ(command->status, NvmeStatus::WRITE_FAULT);
    EXPECT_EQ(payload->get_response_status(), tlm::TLM_GENERIC_ERROR_RESPONSE);
    ReleaseCommand(*payload);
}

TEST(ControllerTest, ReadMissingDataBufferTriggersInvalidField) {
    auto harness = ControllerHarness(MakeDefaultGeometry());

    auto payload = MakePayload(tlm::TLM_READ_COMMAND, nullptr, 4096, 4096);
    AttachCommand(*payload, NvmeOpcode::READ);

    auto delay = sc_core::SC_ZERO_TIME;
    harness.controller_.b_transport(*payload, delay);

    auto* command = payload->get_extension<NvmeCommandExtension>();
    ASSERT_NE(command, nullptr);
    EXPECT_EQ(command->status, NvmeStatus::INVALID_FIELD);
    EXPECT_EQ(payload->get_response_status(), tlm::TLM_GENERIC_ERROR_RESPONSE);
    ReleaseCommand(*payload);
}

TEST(ControllerTest, ReadZeroLengthTriggersInvalidField) {
    auto harness = ControllerHarness(MakeDefaultGeometry());

    auto buffer = std::vector<uint8_t>(4096);
    auto payload =
        MakePayload(tlm::TLM_READ_COMMAND, buffer.data(), 0, buffer.size());
    AttachCommand(*payload, NvmeOpcode::READ);

    auto delay = sc_core::SC_ZERO_TIME;
    harness.controller_.b_transport(*payload, delay);

    auto* command = payload->get_extension<NvmeCommandExtension>();
    ASSERT_NE(command, nullptr);
    EXPECT_EQ(command->status, NvmeStatus::INVALID_FIELD);
    EXPECT_EQ(payload->get_response_status(), tlm::TLM_GENERIC_ERROR_RESPONSE);
    ReleaseCommand(*payload);
}

TEST(ControllerTest, ReadCapacityExceededSetsNvmeStatus) {
    auto harness = ControllerHarness(MakeDefaultGeometry());

    auto buffer = std::vector<uint8_t>(4096);
    auto payload = MakePayload(tlm::TLM_READ_COMMAND, buffer);
    auto* command = AttachCommand(*payload, NvmeOpcode::READ, /*lba=*/1);
    command->length = 1;

    auto delay = sc_core::SC_ZERO_TIME;
    harness.controller_.b_transport(*payload, delay);

    auto* command_after = payload->get_extension<NvmeCommandExtension>();
    ASSERT_NE(command_after, nullptr);
    EXPECT_EQ(command_after->status, NvmeStatus::CAPACITY_EXCEEDED);
    EXPECT_EQ(payload->get_response_status(), tlm::TLM_GENERIC_ERROR_RESPONSE);
    ReleaseCommand(*payload);
}

TEST(ControllerTest, ReadPropagatesNandTransportErrors) {
    auto harness = ControllerHarness(MakeDefaultGeometry());
    harness.nand_.next_response_status_ = tlm::TLM_GENERIC_ERROR_RESPONSE;

    auto buffer = std::vector<uint8_t>(4096);
    auto payload = MakePayload(tlm::TLM_READ_COMMAND, buffer);
    AttachCommand(*payload, NvmeOpcode::READ);

    auto delay = sc_core::SC_ZERO_TIME;
    harness.controller_.b_transport(*payload, delay);

    auto* command = payload->get_extension<NvmeCommandExtension>();
    ASSERT_NE(command, nullptr);
    EXPECT_EQ(command->status, NvmeStatus::INTERNAL_ERROR);
    EXPECT_EQ(payload->get_response_status(), tlm::TLM_GENERIC_ERROR_RESPONSE);
    ReleaseCommand(*payload);
}

TEST(ControllerTest, ReadTranslatesNandFailureToInternalError) {
    auto harness = ControllerHarness(MakeDefaultGeometry());
    harness.nand_.completion_status_ = NandStatus::FAIL;

    auto buffer = std::vector<uint8_t>(4096);
    auto payload = MakePayload(tlm::TLM_READ_COMMAND, buffer);
    AttachCommand(*payload, NvmeOpcode::READ);

    auto delay = sc_core::SC_ZERO_TIME;
    harness.controller_.b_transport(*payload, delay);

    EXPECT_TRUE(harness.nand_.extension_seen_);
    EXPECT_EQ(harness.nand_.last_status_, NandStatus::FAIL);
    auto* command = payload->get_extension<NvmeCommandExtension>();
    ASSERT_NE(command, nullptr);
    EXPECT_EQ(command->status, NvmeStatus::INTERNAL_ERROR);
    EXPECT_EQ(payload->get_response_status(), tlm::TLM_GENERIC_ERROR_RESPONSE);
    ReleaseCommand(*payload);
}

}  // namespace toyssd::test
