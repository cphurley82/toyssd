#include "ssdsim_api.h"
#include <mutex>
#include <queue>
#include <vector>
#include <chrono>

// Forward-declared adapter functions implemented in HostInterface.cpp
namespace ssdsim_internal {
  int submit_cxx(void* user_tag, uint64_t lba, uint32_t size_bytes, bool is_write, void* buf);
  int poll_cxx(int max_cpls, ssd_cpl_t* out);
  int init_cxx(const char* cfg);
  void shutdown_cxx();
}

int ssdsim_init(const char* config_path) {
    return ssdsim_internal::init_cxx(config_path);
}

int ssdsim_submit(const ssd_io_t* req) {
    return ssdsim_internal::submit_cxx(req->user_tag, req->lba, req->size_bytes, req->is_write!=0, req->buf);
}

int ssdsim_poll(int max_cpls, ssd_cpl_t* out) {
    return ssdsim_internal::poll_cxx(max_cpls, out);
}

void ssdsim_shutdown(void) {
    ssdsim_internal::shutdown_cxx();
}
