#pragma once
#include <cstdio>
#include <cstdarg>

inline void log_info(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    fprintf(stdout, "\n");
    va_end(ap);
}
