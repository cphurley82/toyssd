// Minimal fio ioengine that forwards to libssdsim.so
#include "fio.h"
#include <dlfcn.h>
#include <string.h>
#include <stdio.h>
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
    // Hold io_u pointers for completions between getevents() and event()
    struct io_u **events;
    unsigned int nr_events;
    unsigned int queued;
};

static int ssdsim_init(struct thread_data *td)
{
    struct ssdsim_state *st = calloc(1, sizeof(*st));
    td->io_ops_data = st;

    const char* override = getenv("SSD_SIM_LIB_PATH");
    const char* libname = override ? override :
#ifdef __APPLE__
    "./libssdsim.dylib";
#else
    "./libssdsim.so";
#endif
    st->handle = dlopen(libname, RTLD_LAZY | RTLD_GLOBAL);
    if (!st->handle) {
        fprintf(stderr, "ssdsim_engine: dlopen(%s) failed: %s\n", libname, dlerror());
        return 1;
    }

    p_init     = (fn_init_t)dlsym(st->handle, "ssdsim_init");
    p_submit   = (fn_submit_t)dlsym(st->handle, "ssdsim_submit");
    p_poll     = (fn_poll_t)dlsym(st->handle, "ssdsim_poll");
    p_shutdown = (fn_shutdown_t)dlsym(st->handle, "ssdsim_shutdown");

    if (!p_init || !p_submit || !p_poll || !p_shutdown) {
        fprintf(stderr, "ssdsim_engine: dlsym missing: init=%p submit=%p poll=%p shutdown=%p\n",
                (void*)p_init, (void*)p_submit, (void*)p_poll, (void*)p_shutdown);
        return 1;
    }

    // Allocate an events buffer sized to iodepth
    if (td->o.iodepth < 1)
        td->o.iodepth = 1;
    st->events = calloc(td->o.iodepth, sizeof(struct io_u*));
    st->nr_events = 0;

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
    // Track queued IOs so commit() can mark submit
    struct ssdsim_state *st = (struct ssdsim_state*)td->io_ops_data;
    if (st) st->queued++;
    return FIO_Q_QUEUED;
}

static int ssdsim_getevents(struct thread_data *td, unsigned int min, unsigned int max, const struct timespec *t)
{
    struct ssdsim_state *st = (struct ssdsim_state*)td->io_ops_data;
    (void)min; (void)t;
    if (!st) return 0;

    // Cap to our buffer size
    if (max > td->o.iodepth)
        max = td->o.iodepth;

    struct ssd_cpl_t comps[256];
    if (max > 256) max = 256;

    int n = p_poll((int)max, comps);
    if (n < 0) n = 0;
    st->nr_events = (unsigned int)n;
    for (int i = 0; i < n; ++i) {
        struct io_u *io = (struct io_u*)comps[i].user_tag;
        io->error = 0;
        io->resid = 0;
        st->events[i] = io;
    }
    return n;
}

// Return the io_u for the given event index populated by getevents()
static struct io_u* ssdsim_event(struct thread_data *td, int event)
{
    struct ssdsim_state *st = (struct ssdsim_state*)td->io_ops_data;
    if (!st || (unsigned int)event >= st->nr_events)
        return NULL;
    return st->events[event];
}

static int ssdsim_commit(struct thread_data *td)
{
    struct ssdsim_state *st = (struct ssdsim_state*)td->io_ops_data;
    if (st && st->queued) {
        // Inform fio that queued IOs have been submitted asynchronously
        io_u_mark_submit(td, st->queued);
        st->queued = 0;
    }
    return 0;
}

// fio asserts open_file is present even for DISKLESSIO engines; just succeed.
static int ssdsim_open_file(struct thread_data *td, struct fio_file *f)
{
    (void)td; (void)f;
    return 0;
}

static void ssdsim_cleanup(struct thread_data *td)
{
    struct ssdsim_state *st = (struct ssdsim_state*)td->io_ops_data;
    if (p_shutdown) p_shutdown();
    if (st) {
        free(st->events);
        free(st);
    }
}

struct ioengine_ops ioengine = {
    .name       = "ssdsim",
    .version    = FIO_IOOPS_VERSION,
    .flags      = FIO_DISKLESSIO,
    .init       = ssdsim_init,
    .queue      = ssdsim_queue,
    .getevents  = ssdsim_getevents,
    .event      = ssdsim_event,
    .commit     = ssdsim_commit,
    .open_file  = ssdsim_open_file,
    .cleanup    = ssdsim_cleanup,
};

// External ioengine entry point expected by fio when loading modules.
struct ioengine_ops* get_ioengine(void) {
    return &ioengine;
}
