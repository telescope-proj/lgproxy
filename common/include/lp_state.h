// SPDX-License-Identifier: GPL-2.0-or-later
// Telescope Project
// Looking Glass Proxy (LGProxy)
// Copyright (c) 2022 - 2024, Telescope Project Developers

#ifndef LP_STATE_H_
#define LP_STATE_H_

#include <assert.h>
#include <stdint.h>
#include <unordered_set>

namespace lp {

class buffer_sync {

private:
  std::unordered_set<uint8_t> freed;    // Freed at local end
  std::unordered_set<uint8_t> consumed; // Considered used by remote end

public: 

  uint8_t get_consumed() {
    return consumed.size();
  }

  int get_unsynced(uint8_t * out) {
    assert(out);
    int i = 0;
    for (uint8_t e : freed) {
      if (consumed.count(e)) {
        out[i++] = e;
      }
    }
    return i;
  }

  void set_synced(uint8_t * in, uint8_t n) {
    assert(in);
    for (int i = 0; i < n; ++i) {
      consumed.erase(in[i]);
    }
  }

  void set_consumed(uint8_t * in, uint8_t n) {
    assert(in);
    for (int i = 0; i < n; ++i) {
      consumed.insert(in[i]);
    }
  }

  void set_freed(uint8_t * in, uint8_t n) {
    assert(in);
    for (int i = 0; i < n; ++i) {
      freed.insert(in[i]);
    }
  }
};

}; // namespace lp

#endif