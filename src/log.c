#include <stdarg.h>
#include <stdio.h>

#include "log.h"


void anvi_log(enum anvi_log_level level, const char *format, ...) {
    const char *label;
    switch (level) {
        case ANVI_LOG_ERROR:
            label = "error";
            break;
        case ANVI_LOG_DEBUG:
            label = "debug";
            break;
        case ANVI_LOG_INFO:
            label = "info";
            break;
        default:
            label = "unknown";
    }

    fprintf(stderr, "[%s] ", label);

    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fputc('\n', stderr);
    fflush(stderr);
}
