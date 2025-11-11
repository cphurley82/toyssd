// Copyright toyssd contributors
// SPDX-License-Identifier: MIT
//
// File: controller_test.cc
// Brief: Tests host/controller/nand write-read roundtrip and capacity error
//        surfacing via exceptions.

#include <gtest/gtest.h>

#include <tlm>

#include <memory>
#include <numeric>
#include <stdexcept>
#include <vector>

#include "toyssd/controller.hpp"
#include "toyssd/host.hpp"
#include "toyssd/nand.hpp"
#include "tests/test_geometry.hpp"

namespace toyssd::test {

namespace {

constexpr uint32_t kPageSizeBytes = 4096;

// Self-contained stub that lets tests observe how the controller drives the
// NAND socket without spinning up the full NAND model.
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

// Utility that wires a controller under test to the stub NAND. Tests interact
// with the exposed controller_socket directly.
struct ControllerHarness {
  Controller controller_;
  StubNand nand_;

  explicit ControllerHarness(const NandGeometry& geometry)
      : controller_(sc_core::sc_gen_unique_name("controller"), geometry),
        nand_(sc_core::sc_gen_unique_name("stub_nand")) {
    controller_.nand_socket_.bind(nand_.target_socket_);
  }
};

// Attaches an NVMe command extension to the payload.
NvmeCommandExtension* AttachCommand(tlm::tlm_generic_payload& payload,
                                    NvmeOpcode opcode, uint64_t lba = 0) {
  auto* command = new NvmeCommandExtension();
  command->opcode = opcode;
  command->lba = lba;
  command->length = 1;
  payload.set_extension(command);
  return command;
}

// Releases the NVMe command extension currently attached to the payload.
void ReleaseCommand(tlm::tlm_generic_payload& payload) {
  NvmeCommandExtension* released = nullptr;
  payload.release_extension(released);
  delete released;
}

// Configures the payload with the given parameters.
void ConfigurePayload(tlm::tlm_generic_payload& payload,
                      tlm::tlm_command command, uint8_t* data_ptr,
                      size_t data_length, size_t streaming_width) {
  payload.set_command(command);
  payload.set_data_ptr(data_ptr);
  payload.set_data_length(data_length);
  payload.set_streaming_width(streaming_width);
  payload.set_byte_enable_ptr(nullptr);
  payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
}

// Configures the payload with the given parameters.
void ConfigurePayload(tlm::tlm_generic_payload& payload,
                      tlm::tlm_command command, std::vector<uint8_t>& buffer) {
  ConfigurePayload(payload, command, buffer.data(), buffer.size(),
                   buffer.size());
}

// Allocates and initializes a payload with the provided buffer.
std::unique_ptr<tlm::tlm_generic_payload> MakePayload(tlm::tlm_command command,
                                                      uint8_t* data_ptr,
                                                      size_t data_length,
                                                      size_t streaming_width) {
  auto payload = std::make_unique<tlm::tlm_generic_payload>();
  ConfigurePayload(*payload, command, data_ptr, data_length, streaming_width);
  return payload;
}

// Allocates and initializes a payload with the provided buffer.
std::unique_ptr<tlm::tlm_generic_payload> MakePayload(
    tlm::tlm_command command, std::vector<uint8_t>& buffer) {
  return MakePayload(command, buffer.data(), buffer.size(), buffer.size());
}

}  // namespace

// Sanity check that the full host/controller/nand stack can move data.
TEST(ControllerTest, WriteReadRoundtrip) {
  auto geometry = MakeGeometry(
      /*dies=*/1, /*blocks_per_die=*/2, /*pages_per_block=*/4, kPageSizeBytes);

  auto host = Host("host");
  auto controller = Controller("controller", geometry);
  auto nand = Nand("nand", geometry);

  host.nvme_socket.bind(controller.host_socket_);
  controller.nand_socket_.bind(nand.target_socket);

  auto pattern = std::vector<uint8_t>(kPageSizeBytes);
  std::iota(pattern.begin(), pattern.end(), 0);

  host.submit_write(0, pattern, DataPattern::SEQUENTIAL_COUNTER);
  auto result = host.submit_read(0, 1);

  EXPECT_EQ(result, pattern);
}

// Writing beyond the device capacity should raise an error.
TEST(ControllerTest, CapacityExceededTriggersError) {
  auto geometry = MakeGeometry(
      /*dies=*/1, /*blocks_per_die=*/1, /*pages_per_block=*/1, kPageSizeBytes);

  auto host = Host("host");
  auto controller = Controller("controller", geometry);
  auto nand = Nand("nand", geometry);

  host.nvme_socket.bind(controller.host_socket_);
  controller.nand_socket_.bind(nand.target_socket);

  constexpr uint8_t kCapacityPattern = 0xAA;
  auto pattern = std::vector<uint8_t>(kPageSizeBytes, kCapacityPattern);

  EXPECT_THROW(host.submit_write(10, pattern, DataPattern::SEQUENTIAL_COUNTER),
               std::runtime_error);
}

// Constructor should reject obviously invalid geometry before any transport.
TEST(ControllerTest, RejectsGeometryWithZeroDies) {
  auto geometry = MakeGeometry();
  geometry.dies = 0;
  EXPECT_THROW(Controller("controller_invalid_dies", geometry),
               std::invalid_argument);
}

// Constructor should reject obviously invalid geometry before any transport.
TEST(ControllerTest, RejectsGeometryWithZeroBlocksPerDie) {
  auto geometry = MakeGeometry();
  geometry.blocks_per_die = 0;
  EXPECT_THROW(Controller("controller_invalid_blocks", geometry),
               std::invalid_argument);
}

// Constructor should reject obviously invalid geometry before any transport.
TEST(ControllerTest, RejectsGeometryWithZeroPagesPerBlock) {
  auto geometry = MakeGeometry();
  geometry.pages_per_block = 0;
  EXPECT_THROW(Controller("controller_invalid_pages", geometry),
               std::invalid_argument);
}

// Constructor should reject obviously invalid geometry before any transport.
TEST(ControllerTest, RejectsGeometryWithZeroPageSize) {
  auto geometry = MakeGeometry();
  geometry.page_size_bytes = 0;
  EXPECT_THROW(Controller("controller_invalid_page_size", geometry),
               std::invalid_argument);
}

// Missing NVMe command extension should return a generic error.
TEST(ControllerTest, MissingNvmeExtensionReturnsError) {
  auto harness = ControllerHarness(MakeGeometry());

  constexpr uint8_t kMissingExtensionPattern = 0xCD;
  auto data = std::vector<uint8_t>(kPageSizeBytes, kMissingExtensionPattern);
  auto payload = MakePayload(tlm::TLM_WRITE_COMMAND, data);

  auto delay = sc_core::SC_ZERO_TIME;
  harness.controller_.b_transport(*payload, delay);

  EXPECT_EQ(payload->get_response_status(), tlm::TLM_GENERIC_ERROR_RESPONSE);
}

// Invalid opcode in NVMe command extension should set generic error.
TEST(ControllerTest, InvalidOpcodeSetsGenericErrorResponse) {
  auto harness = ControllerHarness(MakeGeometry());

  constexpr uint8_t kInvalidOpcodePattern = 0xCD;
  auto data = std::vector<uint8_t>(kPageSizeBytes, kInvalidOpcodePattern);
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

// Write with missing data buffer should set invalid field status.
TEST(ControllerTest, WriteMissingDataBufferTriggersInvalidField) {
  auto harness = ControllerHarness(MakeGeometry());

  auto payload = MakePayload(tlm::TLM_WRITE_COMMAND, nullptr, kPageSizeBytes,
                             kPageSizeBytes);
  AttachCommand(*payload, NvmeOpcode::WRITE);

  auto delay = sc_core::SC_ZERO_TIME;
  harness.controller_.b_transport(*payload, delay);

  auto* command = payload->get_extension<NvmeCommandExtension>();
  ASSERT_NE(command, nullptr);
  EXPECT_EQ(command->status, NvmeStatus::INVALID_FIELD);
  EXPECT_EQ(payload->get_response_status(), tlm::TLM_GENERIC_ERROR_RESPONSE);
  ReleaseCommand(*payload);
}

// Write with zero length should set invalid field status.
TEST(ControllerTest, WriteZeroLengthTriggersInvalidField) {
  auto harness = ControllerHarness(MakeGeometry());

  auto data = std::vector<uint8_t>(kPageSizeBytes, 0x00);
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

// Write beyond capacity should set capacity exceeded status.
TEST(ControllerTest, WriteCapacityExceededSetsNvmeStatus) {
  auto harness = ControllerHarness(MakeGeometry());

  constexpr uint8_t kCapacityPattern = 0xFF;
  auto data = std::vector<uint8_t>(kPageSizeBytes, kCapacityPattern);
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

// Write should propagate transport errors from NAND.
TEST(ControllerTest, WritePropagatesNandTransportErrors) {
  auto harness = ControllerHarness(MakeGeometry());
  harness.nand_.next_response_status_ = tlm::TLM_GENERIC_ERROR_RESPONSE;

  constexpr uint8_t kTransportErrorPattern = 0x01;
  auto data = std::vector<uint8_t>(kPageSizeBytes, kTransportErrorPattern);
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

// Write should translate NAND failure to write fault status.
TEST(ControllerTest, WriteTranslatesNandFailureToWriteFault) {
  auto harness = ControllerHarness(MakeGeometry());
  harness.nand_.completion_status_ = NandStatus::FAIL;

  constexpr uint8_t kWriteFaultPattern = 0x01;
  auto data = std::vector<uint8_t>(kPageSizeBytes, kWriteFaultPattern);
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

// Read with missing data buffer should set invalid field status.
TEST(ControllerTest, ReadMissingDataBufferTriggersInvalidField) {
  auto harness = ControllerHarness(MakeGeometry());

  auto payload = MakePayload(tlm::TLM_READ_COMMAND, nullptr, kPageSizeBytes,
                             kPageSizeBytes);
  AttachCommand(*payload, NvmeOpcode::READ);

  auto delay = sc_core::SC_ZERO_TIME;
  harness.controller_.b_transport(*payload, delay);

  auto* command = payload->get_extension<NvmeCommandExtension>();
  ASSERT_NE(command, nullptr);
  EXPECT_EQ(command->status, NvmeStatus::INVALID_FIELD);
  EXPECT_EQ(payload->get_response_status(), tlm::TLM_GENERIC_ERROR_RESPONSE);
  ReleaseCommand(*payload);
}

// Read with zero length should set invalid field status.
TEST(ControllerTest, ReadZeroLengthTriggersInvalidField) {
  auto harness = ControllerHarness(MakeGeometry());

  auto buffer = std::vector<uint8_t>(kPageSizeBytes);
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

// Read beyond capacity should set capacity exceeded status.
TEST(ControllerTest, ReadCapacityExceededSetsNvmeStatus) {
  auto harness = ControllerHarness(MakeGeometry());

  auto buffer = std::vector<uint8_t>(kPageSizeBytes);
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

// Read should propagate transport errors from NAND.
TEST(ControllerTest, ReadPropagatesNandTransportErrors) {
  auto harness = ControllerHarness(MakeGeometry());
  harness.nand_.next_response_status_ = tlm::TLM_GENERIC_ERROR_RESPONSE;

  auto buffer = std::vector<uint8_t>(kPageSizeBytes);
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

// Read should translate NAND failure to internal error status.
TEST(ControllerTest, ReadTranslatesNandFailureToInternalError) {
  auto harness = ControllerHarness(MakeGeometry());
  harness.nand_.completion_status_ = NandStatus::FAIL;

  auto buffer = std::vector<uint8_t>(kPageSizeBytes);
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
