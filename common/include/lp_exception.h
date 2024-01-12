// SPDX-License-Identifier: GPL-2.0-or-later
// Telescope Project
// Looking Glass Proxy (LGProxy)
// Copyright (c) 2022 - 2024, Telescope Project Developers

#ifndef LP_EXCEPTION_H_
#define LP_EXCEPTION_H_

#include "lp_log.h"
#include <exception>

/* Exit exception */

#define LP_THROW(retcode, err_str)                                             \
  throw tcm_exception(retcode, _LP_FNAME_, __LINE__, err_str)

enum lp_exit_reason {
  LP_EXIT_LOCAL,
  LP_EXIT_REMOTE
};

class lp_exit : public std::exception {
  lp_exit_reason r;

public:
  lp_exit(lp_exit_reason reason) { r = reason; }
  ~lp_exit() { return; }
  const char * what() {
    switch (r) {
      case LP_EXIT_LOCAL: return "User-requested exit";
      case LP_EXIT_REMOTE: return "Peer-requested exit";
      default: return "Invalid status";
    }
  }
};

#endif