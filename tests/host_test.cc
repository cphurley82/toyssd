// Copyright toyssd contributors
// SPDX-License-Identifier: MIT
//
// File: host_test.cc
// Brief: Exercises Host parameter validation and error propagation.

#include <cstdint>
#include <vector>

#include <tlm>                               // NOLINT(build/include_order)
#include <gtest/gtest.h>                     // NOLINT(build/include_order)
#include <tlm_utils/simple_target_socket.h>  // NOLINT(build/include_order)

#include "toyssd/host.hpp"

namespace toyssd::test {

namespace {

constexpr uint32_t kSectorSizeBytes = 4096;
constexpr uint8_t kWritePattern = 0x5A;
constexpr size_t kMisalignedPayloadBytes = 100;

// Minimal controller stand-in that lets tests dictate response behavior.
class StubController : public sc_core::sc_module {
 public:
  tlm_utils::simple_target_socket<StubController> host_socket_{"host_socket_"};

  explicit StubController(const sc_core::sc_module_name& name)
      : sc_core::sc_module(name) {
    host_socket_.register_b_transport(this, &StubController::b_transport);
  }

  void set_response_status(tlm::tlm_response_status status) {
    next_response_status_ = status;
  }

  void set_completion_status(NvmeStatus status) { completion_status_ = status; }

 private:
  void b_transport(tlm::tlm_generic_payload& payload, sc_core::sc_time& delay) {
    payload.set_response_status(next_response_status_);
    if (auto* command = payload.get_extension<NvmeCommandExtension>();
        command != nullptr) {
      command->status = completion_status_;
    }
    delay = sc_core::SC_ZERO_TIME;
  }

  tlm::tlm_response_status next_response_status_{tlm::TLM_OK_RESPONSE};
  NvmeStatus completion_status_{NvmeStatus::SUCCESS};
};

// Convenience harness that wires a host to the stub controller.
struct HostHarness {
  Host host;
  StubController controller;

  HostHarness()
      : host(sc_core::sc_gen_unique_name("host"), kSectorSizeBytes),
        controller(sc_core::sc_gen_unique_name("stub_controller")) {
    host.nvme_socket.bind(controller.host_socket_);
  }
};

}  // namespace

// Empty write payloads should be rejected before any transport happens.
TEST(HostTest, RejectsEmptyWrite) {
  Host host("host");
  auto data = std::vector<uint8_t>();
  EXPECT_THROW(host.submit_write(0, data, DataPattern::SEQUENTIAL_COUNTER),
               std::invalid_argument);
}

// Payloads must align to the configured sector size.
TEST(HostTest, RejectsMisalignedWrite) {
  Host host("host");
  auto data = std::vector<uint8_t>(kMisalignedPayloadBytes);
  EXPECT_THROW(host.submit_write(0, data, DataPattern::SEQUENTIAL_COUNTER),
               std::invalid_argument);
}

// Controller NVMe failures must surface as runtime_error.
TEST(HostTest, WriteSurfacesNvmeFailure) {
  auto harness = HostHarness();
  harness.controller.set_completion_status(NvmeStatus::INTERNAL_ERROR);
  auto data = std::vector<uint8_t>(kSectorSizeBytes, kWritePattern);
  EXPECT_THROW(
      harness.host.submit_write(0, data, DataPattern::SEQUENTIAL_COUNTER),
      std::runtime_error);
}

// Transport failures must be converted to runtime_error.
TEST(HostTest, WritePropagatesTlmErrors) {
  auto harness = HostHarness();
  harness.controller.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
  auto data = std::vector<uint8_t>(kSectorSizeBytes, kWritePattern);
  EXPECT_THROW(
      harness.host.submit_write(0, data, DataPattern::SEQUENTIAL_COUNTER),
      std::runtime_error);
}

// Zero-length reads are invalid input.
TEST(HostTest, RejectsZeroLengthRead) {
  Host host("host");
  EXPECT_THROW(host.submit_read(0, 0), std::invalid_argument);
}

// Read NVMe failures should throw runtime_error.
TEST(HostTest, ReadSurfacesNvmeFailure) {
  auto harness = HostHarness();
  harness.controller.set_completion_status(NvmeStatus::INTERNAL_ERROR);
  EXPECT_THROW(harness.host.submit_read(0, 1), std::runtime_error);
}

// READ transport failures should also throw runtime_error.
TEST(HostTest, ReadPropagatesTlmErrors) {
  auto harness = HostHarness();
  harness.controller.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
  EXPECT_THROW(harness.host.submit_read(0, 1), std::runtime_error);
}

}  // namespace toyssd::test
