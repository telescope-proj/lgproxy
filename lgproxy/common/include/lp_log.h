/**
 * Copyright (c) 2020 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See `log.c` for details.
 */

#ifndef _LP_LOG_H_
#define _LP_LOG_H_

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <time.h>

#define LP_LOG_VERSION "0.1.0"

#define _LP_FNAME_ (strrchr("/" __FILE__, '/') + 1)

typedef struct {
  va_list ap;
  const char *fmt;
  const char *file;
  struct tm *time;
  void *udata;
  int line;
  int level;
} lp__log_Event;

typedef void (*lp__log_LogFn)(lp__log_Event *ev);
typedef void (*lp__log_LockFn)(bool lock, void *udata);

enum { LP__LOG_TRACE, LP__LOG_DEBUG, LP__LOG_INFO, LP__LOG_WARN, LP__LOG_ERROR, LP__LOG_FATAL };

#define lp__log_trace(...) lp__log_log(LP__LOG_TRACE, _LP_FNAME_, __LINE__, __VA_ARGS__)
#define lp__log_debug(...) lp__log_log(LP__LOG_DEBUG, _LP_FNAME_, __LINE__, __VA_ARGS__)
#define lp__log_info(...)  lp__log_log(LP__LOG_INFO,  _LP_FNAME_, __LINE__, __VA_ARGS__)
#define lp__log_warn(...)  lp__log_log(LP__LOG_WARN,  _LP_FNAME_, __LINE__, __VA_ARGS__)
#define lp__log_error(...) lp__log_log(LP__LOG_ERROR, _LP_FNAME_, __LINE__, __VA_ARGS__)
#define lp__log_fatal(...) lp__log_log(LP__LOG_FATAL, _LP_FNAME_, __LINE__, __VA_ARGS__)

const char* lp__log_level_string(int level);
void lp__log_set_lock(lp__log_LockFn fn, void *udata);
void lp__log_set_level(int level);
void lp__log_set_quiet(bool enable);
int lp__log_add_callback(lp__log_LogFn fn, void *udata, int level);
int lp__log_add_fp(FILE *fp, int level);

void lp__log_log(int level, const char *file, int line, const char *fmt, ...);

#endif
