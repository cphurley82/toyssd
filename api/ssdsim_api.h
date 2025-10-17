#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void*    user_tag;
    uint64_t lba;
    uint32_t size_bytes;
    int      is_write;
    void*    buf;
} ssd_io_t;

typedef struct {
    void*    user_tag;
    int      status;
    uint64_t ns;
} ssd_cpl_t;

int ssdsim_init(const char* config_path);
int ssdsim_submit(const ssd_io_t* req);       // non-blocking
int ssdsim_poll(int max_cpls, ssd_cpl_t* out);// returns number of completions
void ssdsim_shutdown(void);

#ifdef __cplusplus
}
#endif
