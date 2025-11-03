// Copyright Chris Hurley
#pragma once
#include <cstdint>
#include <mutex>
#include <queue>
#include <vector>

#include "api/ssdsim_api.h"
#include "sim/util/compat.h"
#include "sim/util/constants.h"
#include "systemc"

struct IORequest {
  void* user_tag;
  uint64_t lba;
  uint32_t size_bytes;
  bool is_write;
  uint8_t* buf;
  sc_core::sc_time submit_ts;
};
struct Completion {
  void* user_tag;
  int status;
  sc_core::sc_time complete_ts;
  sc_core::sc_time latency{sc_core::SC_ZERO_TIME};
};

inline std::ostream& operator<<(std::ostream& os, const Completion& c) {
  return os << "Completion{" << c.user_tag << ", status=" << c.status
            << ", t=" << c.complete_ts << ", latency=" << c.latency << "}";
}

struct HostInterface : sc_core::sc_module {
  sc_core::sc_fifo<IORequest*> to_fw{toyssd::constants::kDefaultFifoSize};
  sc_core::sc_fifo<Completion> from_fw{toyssd::constants::kDefaultFifoSize};

  SC_CTOR(HostInterface) {}

  int submit(const IORequest& request);
  int poll(int max_completions, ssd_cpl_t* out);
};

// C API adapters
namespace ssdsim_internal {
int submit_cxx(void* user_tag, uint64_t lba, uint32_t size_bytes, bool is_write,
               void* buf);
int poll_cxx(int max_cpls, ssd_cpl_t* out_cpls);
int init_cxx(const char* cfg);
void shutdown_cxx();
}  // namespace ssdsim_internal
