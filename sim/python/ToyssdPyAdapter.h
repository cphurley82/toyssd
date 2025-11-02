// Copyright Chris Hurley
#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "api/ssdsim_api.h"
#include "sim/Top.h"
#include "sim/host/HostInterface.h"
#include "sim/nand/NandModel.h"
#include "systemc"

namespace toyssd::python {

struct PyCompletion {
  uint64_t request_id{0};
  int status{0};
  uint64_t completion_ns{0};
};

class ToyssdPyAdapter {
 public:
  ToyssdPyAdapter();
  ~ToyssdPyAdapter();

  ToyssdPyAdapter(const ToyssdPyAdapter&) = delete;
  ToyssdPyAdapter& operator=(const ToyssdPyAdapter&) = delete;

  uint64_t submit_write(uint64_t lba, uint32_t size_bytes);
  uint64_t submit_read(uint64_t lba, uint32_t size_bytes);

  std::vector<PyCompletion> poll(int max_completions);
  std::vector<NandModel::Event> drain_nand_events();

  void run_for_ns(uint64_t ns);
  void run_for_us(uint64_t us);
  void run_for_ms(uint64_t ms);

  uint64_t time_ps() const;

  void reset();

 private:
  struct RequestToken {
    uint64_t id{0};
    bool is_write{false};
  };

  uint64_t submit(uint64_t lba, uint32_t size_bytes, bool is_write);

  void initialize_top();

  std::unique_ptr<sc_core::sc_simcontext> simctx_;
  std::unique_ptr<Top> top_;
  HostInterface* host_{nullptr};

  uint64_t next_id_{0};
  std::unordered_map<uint64_t, std::unique_ptr<RequestToken>> tokens_;
  std::unordered_map<uint64_t, std::vector<uint8_t>> payloads_;
};

}  // namespace toyssd::python
