#include "host/HostInterface.h"
#include "systemc"
#include "../fw/Firmware.h"
#include "../Top.h"
#include <memory>
#include <cstring>

using namespace sc_core;


static HostInterface* g_host = nullptr;

int HostInterface::submit(const IORequest& r) {
    auto *req = new IORequest(r);
    req->submit_ts = sc_time_stamp();
    if (!to_fw.nb_write(req)) {
        delete req;
        return 1; // busy
    }
    return 0;
}

int HostInterface::poll(int max_cpls, ssd_cpl_t* out) {
    int n = 0;
    Completion c;
    while (n < max_cpls && from_fw.nb_read(c)) {
        out[n].user_tag = c.user_tag;
        out[n].status = c.status;
        out[n].ns = (c.complete_ts.value() - SC_ZERO_TIME.value()); // raw ticks
        ++n;
    }
    return n;
}

// --------------- C API adapters ---------------
namespace ssdsim_internal {
    static std::unique_ptr<sc_core::sc_simcontext> simctx;

  int init_cxx(const char* /*cfg*/) {
      if (g_host) return 0;
      // Build a minimal topology similar to sc_main
      simctx.reset(new sc_simcontext());
      // Construct a Top in this translation unit (defined in sim/main.cpp)
    auto* top = ssdsim_internal::create_top(&g_host);
            // End of elaboration and initialize delta cycles
            sc_start(SC_ZERO_TIME);
            // don't start an infinite loop; simulation advances on poll
      return 0;
  }

  int submit_cxx(void* user_tag, uint64_t lba, uint32_t size_bytes, bool is_write, void* buf) {
      if (!g_host) return 1;
      IORequest r{user_tag, lba, size_bytes, is_write, (uint8_t*)buf, sc_time_stamp()};
      return g_host->submit(r);
  }

    int poll_cxx(int max_cpls, ssd_cpl_t* out_cpls) {
            if (!g_host) return 0;
            // Try to advance simulation in small quanta until we have at least one completion
            // or we hit a small number of iterations to avoid blocking.
            int total = 0;
            for (int iter = 0; iter < 100 && total < max_cpls; ++iter) {
                // Advance simulation time by a small amount; adjust as needed
                sc_start(sc_time(10, SC_US));
                // Drain completions from firmware
                int n = g_host->poll(max_cpls - total, out_cpls + total);
                if (n > 0) {
                    total += n;
                    // continue loop to try to fill up to max_cpls
                }
            }
            return total;
  }

  void shutdown_cxx() {
      simctx.reset();
      g_host = nullptr;
  }
}
