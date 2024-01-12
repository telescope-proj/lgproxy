#ifndef LP_TIME_H_
#define LP_TIME_H_

#include "lp_exception.h"
#include "lp_log.h"
#include <stdint.h>
#include <time.h>

#define LP_NOW -2

static inline float lp_tdiff_to_float(const timespec & start,
                                      const timespec & stop) {
  float e = (float) stop.tv_sec + (float) stop.tv_nsec / 1000000000.0f;
  float s = (float) start.tv_sec + (float) start.tv_nsec / 1000000000.0f;
  return s - e;
}

static inline timespec lp_get_deadline(int64_t timeout_ms) {
  timespec t;

  int ret = clock_gettime(CLOCK_MONOTONIC, &t);
  if (ret < 0) {
    LP_THROW(errno, "clock_gettime() failed");
  }

  switch (timeout_ms) {
    case 0:
      t.tv_sec  = 0;
      t.tv_nsec = 0;
      return t;
    case -1:
      t.tv_sec  = (time_t) -1;
      t.tv_nsec = (time_t) -1;
      return t;
    case -2: return t;
    default:
      if (timeout_ms > 0) {
        t.tv_sec += (timeout_ms / 1000);
        t.tv_nsec += ((timeout_ms % 1000) * 1000000);
        if (t.tv_nsec > 1000000000) {
          t.tv_sec++;
          t.tv_nsec -= 1000000000;
        }
        return t;
      }
      LP_THROW(EINVAL, "Invalid timeout");
  }
}

static inline int lp_check_deadline(const timespec & dl) {
  struct timespec t;
  if (dl.tv_sec == 0 && dl.tv_nsec == 0) {
    return 1; // Single-poll value
  } else if (dl.tv_sec < 0 && dl.tv_nsec < 0) {
    return 0; // No timeout
  } else {
    int ret = clock_gettime(CLOCK_MONOTONIC, &t);
    if (ret) {
      LP_THROW(errno, "clock_gettime() failed");
    }
    return (t.tv_sec > dl.tv_sec) ||
           ((t.tv_sec == dl.tv_sec) && (t.tv_nsec > dl.tv_nsec));
  }
}

#endif