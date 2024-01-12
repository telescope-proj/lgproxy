// SPDX-License-Identifier: GPL-2.0-or-later
// Telescope Project
// Looking Glass Proxy (LGProxy)
// Copyright (c) 2022 - 2024, Telescope Project Developers

#ifndef LP_LGMP_SERVER_H_
#define LP_LGMP_SERVER_H_

#include <fcntl.h>
#include <rdma/fabric.h>
#include <sys/ioctl.h>

#include "lp_mem.h"
#include "lp_metadata.h"
#include "lp_types.h"

extern "C" {
#include "common/KVMFR.h"
#include "lgmp/host.h"
#include "lgmp/lgmp.h"
#include "module/kvmfr.h"
}

#include "nfr_protocol.h"
#include "nfr_vmsg.hpp"

#include <memory>
#include <queue>
#include <vector>

#if defined(__GNUC__)
#define restrict __restrict__
#elif defined(__clang__)
#define restrict __restrict__
#else
#define restrict
#endif

struct lp_deleter {
  void operator()(void * ptr) { free(ptr); }
};

enum lp_buffer_type {
  BUFFER_INVALID,
  BUFFER_FRAME,
  BUFFER_CURSOR,
  BUFFER_CURSOR_SHAPE
};

enum mem_state {
  M_FREE,     // Unused
  M_RESERVED, // Reserved, not pushed into the LGMP queue
  M_PENDING   // Pushed into the LGMP queue, pending
};

static inline int64_t get_max_frame_size(int64_t shm_len) {
  shm_len -= MAX_POINTER_SIZE_ALIGN * POINTER_SHAPE_BUFFERS;
  shm_len -= LGMP_Q_POINTER_LEN * sizeof(KVMFRCursor);
  if (shm_len < 0)
    return -ENOBUFS;
  int64_t pgs = shm_len / LGMP_Q_FRAME_LEN / tcm_get_page_size();
  if (pgs <= 0)
    return -ENOBUFS;
  return pgs * tcm_get_page_size();
}

class lp_lgmp_host_q;

struct lp_lgmp_mem {

private:
  lp_lgmp_host_q * q;

public:
  std::vector<PLGMPMemory> mem;
  std::vector<mem_state>   states;
  size_t                   itm_size; // ... of an individual element!
  size_t                   alignment;

  lp_lgmp_mem();

  void clear();
  void bind(lp_lgmp_host_q * q);
  bool alloc(uint32_t size, uint32_t alignment = 0, uint32_t n = 1);

  PLGMPMemory operator[](int index);

  int      reserve(int index = -1);
  void     release(int index);
  uint32_t reclaim(PLGMPHostQueue q, uint8_t * out);

  void export_buffers(void ** out);
  void export_offsets(NFROffset * out);

  ~lp_lgmp_mem();
};

class lp_lgmp_host_q {

public:
  PLGMPHost      host;
  PLGMPHostQueue frame_q;
  PLGMPHostQueue ptr_q;

  lp_lgmp_mem mem_frame;
  lp_lgmp_mem mem_ptr;
  lp_lgmp_mem mem_shape;

  std::shared_ptr<lp_shmem> mem;

  std::unique_ptr<void, lp_deleter> udata;
  uint64_t                          udata_size;
  uint16_t                          init_flags;

  void clear_fields();

  void release_resources();

  /**
   * @brief Bind a LGMP host queue object to a shared memory region.
   *
   * This function does not initialize the LGMP session. Use the reset()
   * function for this.
   *
   */
  lp_lgmp_host_q(std::shared_ptr<lp_shmem> & mem);

  ~lp_lgmp_host_q();

  /**
   * @brief (Re-)initialize the LGMP host.
   *
   * @param mtd Metadata received by source application
   * @param feature_flags KVMFR feature flags
   * @return 0 on success, negative error code on failure - if an LGMP error
   *         occurred, errno will be set to the LGMP-specific error code.
   */
  int reset(lp_metadata & mtd, uint32_t feature_flags);
  int reset(void * udata, size_t udata_size, bool copy = true);
  int process(void * out, size_t * size);

  int num_clients(uint32_t q_id);

  LGMP_STATUS post_cursor(PLGMPMemory m, uint32_t udata);
  LGMP_STATUS post_frame(PLGMPMemory m, uint32_t udata);
};

#endif