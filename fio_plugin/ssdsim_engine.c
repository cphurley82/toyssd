// Minimal fio ioengine that forwards to libssdsim.so
#include "fio.h"
#include <dlfcn.h>
#include <string.h>
#include <stdlib.h>

typedef int (*fn_init_t)(const char*);
typedef int (*fn_submit_t)(const void*);
typedef int (*fn_poll_t)(int, void*);
typedef void (*fn_shutdown_t)(void);

static fn_init_t      p_init;
static fn_submit_t    p_submit;
static fn_poll_t      p_poll;
static fn_shutdown_t  p_shutdown;

struct ssd_io_t {
    void* user_tag;
    uint64_t lba;
    uint32_t size_bytes;
    int is_write;
    void* buf;
};

struct ssd_cpl_t {
    void* user_tag;
    int status;
    uint64_t ns;
};

struct ssdsim_state {
    void* handle;
};

static int ssdsim_init(struct thread_data *td)
{
    struct ssdsim_state *st = calloc(1, sizeof(*st));
    td->io_ops_data = st;

    const char* libname =
#ifdef __APPLE__
    "./libssdsim.dylib";
#else
    "./libssdsim.so";
#endif
    st->handle = dlopen(libname, RTLD_LAZY);
    if (!st->handle) return 1;

    p_init     = (fn_init_t)dlsym(st->handle, "ssdsim_init");
    p_submit   = (fn_submit_t)dlsym(st->handle, "ssdsim_submit");
    p_poll     = (fn_poll_t)dlsym(st->handle, "ssdsim_poll");
    p_shutdown = (fn_shutdown_t)dlsym(st->handle, "ssdsim_shutdown");

    if (!p_init || !p_submit || !p_poll || !p_shutdown) return 1;

    // fio >= 3.x keeps options under td->o
    const char* cfg = td->o.filename ? td->o.filename : "config/default.json";
    return p_init(cfg);
}

static enum fio_q_status ssdsim_queue(struct thread_data *td, struct io_u *io)
{
    struct ssd_io_t req = {
        .user_tag   = io,
        .lba        = io->offset / 512,
        .size_bytes = (uint32_t)io->xfer_buflen,
        .is_write   = (io->ddir == DDIR_WRITE),
        .buf        = io->xfer_buf
    };
    int rc = p_submit(&req);
    if (rc) return FIO_Q_BUSY;
    return FIO_Q_QUEUED;
}

static int ssdsim_getevents(struct thread_data *td, unsigned int min, unsigned int max, const struct timespec *t)
{
    struct ssd_cpl_t comps[256];
    int n = p_poll((int)max, comps);
    for (int i=0; i<n; ++i) {
        struct io_u *io = (struct io_u*)comps[i].user_tag;
        io->error = 0;
        io->resid = 0;
        io_u_mark_complete(td, io->index);
    }
    return n;
}

static int ssdsim_commit(struct thread_data *td)
{
    return 0;
}

static void ssdsim_cleanup(struct thread_data *td)
{
    if (p_shutdown) p_shutdown();
    if (td->io_ops_data) free(td->io_ops_data);
}

static struct ioengine_ops ioengine_ssdsim = {
    .name       = "ssdsim",
    .version    = FIO_IOOPS_VERSION,
    .flags      = FIO_DISKLESSIO,
    .init       = ssdsim_init,
    .queue      = ssdsim_queue,
    .getevents  = ssdsim_getevents,
    .commit     = ssdsim_commit,
    .cleanup    = ssdsim_cleanup,
};

static void fio_init ssdsim_register(void)
{
    register_ioengine(&ioengine_ssdsim);
}
