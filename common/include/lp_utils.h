// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef LP_UTILS_H_
#define LP_UTILS_H_

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <sys/stat.h>

#include "lp_log.h"
#include "lp_types.h"
#include "tcm_log.h"

/* Instead of allocating memory for each RDMA operation, we split the single
 * 64-bit integer allocated for context into multiple sections. */
#define CONTEXT(operation, offset)                                             \
  (void *) (uintptr_t) (((uintptr_t) operation << 56) |                        \
                        ((uintptr_t) offset & 0xFFFFFFFFFFFFFF))

#define GET_CONTEXT_OP(val) ((((uintptr_t) val) >> 56) & 0xFF)
#define GET_CONTEXT_OFFSET(val) ((uintptr_t) val & 0xFFFFFFFFFFFFFF)

/* Free a pointer and unset it, to avoid double frees */
#define lp_free_unset(x)                                                       \
  free(x);                                                                     \
  x = 0;

/* Copy a string into a fixed-size buffer, filling unused bytes with 0 and
 * always null-terminating dst, even if the string would be truncated. */
static inline void lp_strfcpy(char * dst, const char * src, size_t n) {
  assert(dst);
  assert(src);
  assert(n);
  strncpy(dst, src, n - 1);
  dst[n - 1] = '\0';
}

static inline size_t lp_get_page_size() {
#if defined(_WIN32)
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  return si.dwPageSize;
#else
  return sysconf(_SC_PAGESIZE);
#endif
}

/**
 * @brief Parse byte amount with units from string
 *
 * @param data          String containing memory amount (e.g. 1048576 or 128m)
 * @return              Actual size in bytes, -1 if invalid data
 */
uint64_t lp_parse_mem_string(char * data);

/**
 * @brief Set Looking Glass Proxy logging level
 */
static inline void lp_set_log_level() {
  char * loglevel = getenv("LP_LOG_LEVEL");
  if (!loglevel) {
    lp_log_set_level(LP_LOG_INFO);
  } else {
    int ll = atoi(loglevel);
    switch (ll) {
      case 1: lp_log_set_level(LP_LOG_TRACE); break;
      case 2: lp_log_set_level(LP_LOG_DEBUG); break;
      case 3: lp_log_set_level(LP_LOG_INFO); break;
      case 4: lp_log_set_level(LP_LOG_WARN); break;
      case 5: lp_log_set_level(LP_LOG_ERROR); break;
      case 6: lp_log_set_level(LP_LOG_FATAL); break;
      default: lp_log_set_level(LP_LOG_INFO); break;
    }
  }
}

#endif