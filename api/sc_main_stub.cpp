// Provide a dummy sc_main so dlopen() of libssdsim works even when
// SystemC is linked but no sc_main is present in the hosting process.
// Real entrypoints are provided by GoogleTest (tests) or sim_main.
//
// IMPORTANT: On Linux, libsystemc may reference an unmangled symbol named
// 'sc_main' at load time. Export it with C linkage so the dynamic linker
// can resolve it (e.g., via LD_PRELOAD or when present in the main binary).
extern "C" {
#if defined(__GNUC__) || defined(__clang__)
__attribute__((visibility("default")))
#endif
int sc_main(int, char**) {
    return 0;
}
}
