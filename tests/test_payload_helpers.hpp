// Copyright toyssd contributors
// SPDX-License-Identifier: MIT
//
// Shared helpers for constructing tlm_generic_payload instances and common
// test buffers to keep suites consistent.

#pragma once

#include <tlm>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <numeric>
#include <vector>

namespace toyssd::test {

// Creates a payload with explicit buffer metadata (used when tests provide raw
// pointers from fixture-owned buffers).
inline std::unique_ptr<tlm::tlm_generic_payload> MakePayload(
    tlm::tlm_command command, uint8_t* data_ptr, size_t data_length,
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

// Convenience overload for cases where tests work with std::vector buffers.
inline std::unique_ptr<tlm::tlm_generic_payload> MakePayload(
    tlm::tlm_command command, std::vector<uint8_t>& buffer) {
  return MakePayload(command, buffer.data(), buffer.size(), buffer.size());
}

// Releases and deletes the extension attached to the payload, preventing leaks.
template <typename Extension>
inline void ReleaseExtension(tlm::tlm_generic_payload& payload) {
  Extension* released = nullptr;
  payload.release_extension(released);
  delete released;
}

// Returns a buffer filled with a single byte pattern (e.g., for error cases).
inline std::vector<uint8_t> MakePatternBuffer(size_t size, uint8_t value) {
  return std::vector<uint8_t>(size, value);
}

// Returns a monotonic counter buffer starting at the provided seed, useful for
// round trip comparisons where unique content helps detect corruption.
inline std::vector<uint8_t> MakeSequentialBuffer(size_t size,
                                                 uint8_t start = 0) {
  auto buffer = std::vector<uint8_t>(size);
  std::iota(buffer.begin(), buffer.end(), start);
  return buffer;
}

}  // namespace toyssd::test
