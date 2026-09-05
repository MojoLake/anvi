#ifndef ANVI_LOG_H
#define ANVI_LOG_H

enum anvi_log_level {
    ANVI_LOG_ERROR,
    ANVI_LOG_DEBUG,
    ANVI_LOG_INFO,
};

void anvi_log(enum anvi_log_level level, const char *format, ...);

#ifdef ANVI_DEBUG_LOGGING
#define anvi_log_debug(...) anvi_log(ANVI_LOG_DEBUG, __VA_ARGS__)
#else
#define anvi_debug(...) ((void)0)
#endif

#define anvi_log_error(...) anvi_log(ANVI_LOG_ERROR, __VA_ARGS__)
#define anvi_log_info(...) anvi_log(ANVI_LOG_INFO, __VA_ARGS__)

#endif
