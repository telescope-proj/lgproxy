/**
 * Copyright (c) 2020 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See `log.c` for details.
 */

#ifndef LP_LOG_H_
#define LP_LOG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define LP_LOG_VERSION "0.1.0"

#define _LP_FNAME_ (strrchr("/" __FILE__, '/') + 1)

typedef struct {
    va_list      ap;
    const char * fmt;
    const char * file;
    struct tm *  time;
    void *       udata;
    int          line;
    int          level;
} lp_log_Event;

typedef void (*lp_log_LogFn)(lp_log_Event * ev);
typedef void (*lp_log_LockFn)(bool lock, void * udata);

enum {
    LP_LOG_TRACE,
    LP_LOG_DEBUG,
    LP_LOG_INFO,
    LP_LOG_WARN,
    LP_LOG_ERROR,
    LP_LOG_FATAL
};

#define lp_log_trace(...)                                                     \
    lp_log_log(LP_LOG_TRACE, _LP_FNAME_, __LINE__, __VA_ARGS__)
#define lp_log_debug(...)                                                     \
    lp_log_log(LP_LOG_DEBUG, _LP_FNAME_, __LINE__, __VA_ARGS__)
#define lp_log_info(...)                                                      \
    lp_log_log(LP_LOG_INFO, _LP_FNAME_, __LINE__, __VA_ARGS__)
#define lp_log_warn(...)                                                      \
    lp_log_log(LP_LOG_WARN, _LP_FNAME_, __LINE__, __VA_ARGS__)
#define lp_log_error(...)                                                     \
    lp_log_log(LP_LOG_ERROR, _LP_FNAME_, __LINE__, __VA_ARGS__)
#define lp_log_fatal(...)                                                     \
    lp_log_log(LP_LOG_FATAL, _LP_FNAME_, __LINE__, __VA_ARGS__)

const char * lp_log_level_string(int level);
void         lp_log_set_lock(lp_log_LockFn fn, void * udata);
void         lp_log_set_level(int level);
void         lp_log_set_quiet(bool enable);
int          lp_log_add_callback(lp_log_LogFn fn, void * udata, int level);
int          lp_log_add_fp(FILE * fp, int level);

void lp_log_log(int level, const char * file, int line, const char * fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
