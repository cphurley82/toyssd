// Copyright Chris Hurley
#include "sim/host/host_interface.h"

#include <cmath>
#include <cstring>
#include <memory>
#include <mutex>

#include "sim/fw/firmware.h"
#include "sim/top.h"
#include "sim/util/compat.h"
#include "sim/util/constants.h"
#include "systemc"

// Avoid using-directives; use explicit sc_core:: qualifiers.

static HostInterface* g_host = nullptr;

int HostInterface::submit(const IORequest& request) {
  auto* req = new IORequest(request);
  req->submit_ts = sc_core::sc_time_stamp();
  if (!to_fw.nb_write(req)) {
    delete req;
    return 1;  // busy
  }
  return 0;
}

int HostInterface::poll(int max_completions, ssd_cpl_t* out) {
  int produced = 0;
  Completion cpl;
  while (produced < max_completions && from_fw.nb_read(cpl)) {
    out[produced].user_tag = cpl.user_tag;
    out[produced].status = cpl.status;
    double latency_ns =
        cpl.latency.to_seconds() * toyssd::constants::kNanosecondsPerSecond;
    if (latency_ns < 0.0) {
      latency_ns = 0.0;
    }
    out[produced].ns = static_cast<uint64_t>(std::llround(latency_ns));
    ++produced;
  }
  return produced;
}

// --------------- C API adapters ---------------
namespace ssdsim_internal {
static std::unique_ptr<sc_core::sc_simcontext> simctx;
static std::mutex sim_mutex;

int init_cxx(const char* /*cfg*/) {
  std::lock_guard<std::mutex> lock(sim_mutex);
  if (g_host != nullptr) {
    return 0;
  }
  // Build a minimal topology similar to sc_main
  simctx = std::make_unique<sc_core::sc_simcontext>();
  // Construct a Top in this translation unit (defined in sim/main.cc)
  ssdsim_internal::create_top(&g_host);
  // End of elaboration and initialize delta cycles
  sc_core::sc_start(sc_core::SC_ZERO_TIME);
  // don't start an infinite loop; simulation advances on poll
  return 0;
}

int submit_cxx(void* user_tag, uint64_t lba, uint32_t size_bytes, bool is_write,
               void* buf) {
  std::lock_guard<std::mutex> lock(sim_mutex);
  if (g_host == nullptr) {
    return 1;
  }
  IORequest req{
      .user_tag = user_tag,
      .lba = lba,
      .size_bytes = size_bytes,
      .is_write = is_write,
      .buf = reinterpret_cast<uint8_t*>(buf),
      .submit_ts = sc_core::sc_time_stamp(),
  };
  return g_host->submit(req);
}

int poll_cxx(int max_cpls, ssd_cpl_t* out_cpls) {
  std::lock_guard<std::mutex> lock(sim_mutex);
  if (g_host == nullptr) {
    return 0;
  }
  // Try to advance simulation in small quanta until we have at least one
  // completion or we hit a small number of iterations to avoid blocking.
  int total = 0;
  for (int iter = 0;
       iter < toyssd::constants::kPollMaxIterations && total < max_cpls;
       ++iter) {
    // Advance simulation time by a small amount; adjust as needed
    sc_core::sc_start(sc_core::sc_time(toyssd::constants::kPollStepMicroseconds,
                                       sc_core::SC_US));
    // Drain completions from firmware
    const int newly_produced = g_host->poll(max_cpls - total, out_cpls + total);
    if (newly_produced > 0) {
      total += newly_produced;
      // continue loop to try to fill up to max_cpls
    }
  }
  return total;
}

void shutdown_cxx() {
  std::lock_guard<std::mutex> lock(sim_mutex);
  simctx = nullptr;
  g_host = nullptr;
}
}  // namespace ssdsim_internal
