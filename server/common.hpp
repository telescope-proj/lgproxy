// SPDX-License-Identifier: GPL-2.0-or-later
// Telescope Project
// Looking Glass Proxy (LGProxy)
// Copyright (c) 2022 - 2024, Telescope Project Developers

#ifndef LP_SERVER_COMMON_HPP_
#define LP_SERVER_COMMON_HPP_

#include "lgmp_client.hpp"

#include "tcm_conn.h"
#include "tcm_errno.h"
#include "tcm_fabric.h"

#include "lp_config.h"
#include "lp_exception.h"
#include "lp_metadata.h"
#include "lp_queue.h"
#include "lp_state.h"
#include "lp_time.h"
#include "lp_types.h"
#include "lp_utils.h"

#include "nfr_server.hpp"
#include "nfr_util.hpp"

#include <memory>
#include <queue>
#include <thread>

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>

using std::queue;
using std::shared_ptr;
using std::unique_ptr;

#define MAX_PENDING_MSG 32
#define MAX_PENDING_FRAME 2
#define MAX_PENDING_CURSOR (LGMP_Q_POINTER_LEN + POINTER_SHAPE_BUFFERS)

enum buffer_tag : uint8_t {
  TAG_INVALID,
  TAG_MSG_RX,
  TAG_MSG_TX,
  TAG_FRAME_TX,
  TAG_FRAME_RX,
  TAG_FRAME_WRITE,
  TAG_CURSOR_SHAPE,
  TAG_MAX
};

static inline const char * get_buffer_tag_str(uint8_t tag) {
  switch (tag) {
    case TAG_MSG_RX: return "MSG_RX";
    case TAG_MSG_TX: return "MSG_TX";
    case TAG_FRAME_RX: return "FRAME_RX";
    case TAG_FRAME_TX: return "FRAME_TX";
    case TAG_FRAME_WRITE: return "FRAME_WRITE";
    case TAG_CURSOR_SHAPE: return "CURSOR_SHAPE";
    default: return "?";
  }
}

enum system_flags : uint32_t {

  // Active
  S_ACTIVE = (1 << 0), // Relay ready and active

  // Initialization
  S_F_INIT = (1 << 1), // Frame thread initializing
  S_C_INIT = (1 << 2), // Cursor thread initializing

  // Local relay pause (frame channel)
  S_F_PAUSE_LOCAL_UNSYNC = (1 << 3), // Pause, peer uninformed
  S_F_PAUSE_LOCAL_SYNC   = (1 << 4), // Pause, peer informed

  // Local relay pause (cursor channel)
  S_C_PAUSE_LOCAL_UNSYNC = (1 << 5), // Pause, peer uninformed
  S_C_PAUSE_LOCAL_SYNC   = (1 << 6), // Pause, peer informed

  // Remote pause
  S_F_PAUSE_REMOTE = (1 << 7), // Frame channel remote pause
  S_C_PAUSE_REMOTE = (1 << 8), // Cursor channel remote pause

  // Global exit control
  S_EXIT = (1 << 30)
  // If both sides are in a pause state, the flags can both be set
};

enum thread_id {
  THREAD_CURSOR,
  THREAD_FRAME
};

enum profiler_state {
  PROFILER_STOP,
  PROFILER_STOP_ACK,
  PROFILER_START,
  PROFILER_START_ACK,
  PROFILER_EXIT,
  PROFILER_EXIT_ACK
};

// Shared resources / parameters

struct shared_state {
  int64_t                   lgmp_timeout;
  int64_t                   lgmp_interval;
  int64_t                   net_timeout;
  int64_t                   net_interval;
  shared_ptr<lp_rdma_shmem> rshm;
  pthread_mutex_t *         start_lock;
  NFRServerResource *       resrc;
  std::atomic<uint32_t>     ctrl;

  // Profiler-specific

  std::atomic<uint8_t>      profiler_lock;
  std::atomic<const char *> core_state_str;
  std::atomic<const char *> cursor_state_str;
  std::atomic<const char *> frame_state_str;
};

// Frame thread-specific

struct frame_context {
  int8_t     index;
  bool       ack;
  bool       written;
  KVMFRFrame fi;
};

struct pending_frame_meta {
  bool             pending;
  NFRFrameMetadata mtd;
};

struct t_state_frame {
  lp_rbuf                      remote_fb;
  unique_ptr<lp_lgmp_client_q> lgmp;
  unique_ptr<lp_mbuf>          mbuf;
  shared_ptr<tcm_cq>           cq;
  bool                         subbed;
  bool                         wait_fb;
  bool                         no_remote_fb; // No available remote FBs
  timespec                     net_deadline;
  timespec                     lgmp_deadline;

  lp::fixed_deque<fi_cq_data_entry, MAX_PENDING_MSG>     unproc_q;
  lp::fixed_deque<lp_lgmp_msg, MAX_PENDING_MSG>          unproc_lgmp;
  lp::fixed_deque<pending_frame_meta, MAX_PENDING_FRAME> unproc_frame;
};

// Cursor thread-specific

struct cursor_context {
  int16_t  x, y;   // Position x, y
  int16_t  hx, hy; // Hotspot x,y
  uint32_t flags;  // KVMFR flags

  // Optional buffer information

  uint16_t w, h; // Cursor shape buffer width/height
  uint32_t rb;   // Row bytes
  int8_t   ridx; // Remote buffer index (if any)
  int8_t   lidx; // Local buffer index (if any)
};

struct t_state_cursor {
  lp_rbuf                      remote_cb; // Remote cursor buffer metadata
  unique_ptr<lp_lgmp_client_q> lgmp;      // LGMP session
  unique_ptr<lp_mbuf>          mbuf;      // Message buffer
  shared_ptr<tcm_cq>           cq;
  bool                         subbed;      // LGMP subscribed
  bool                         pause_req;   // Pause requested by client
  bool                         pause_local; // Pause due to no data available
  timespec                     net_deadline;
  timespec                     lgmp_deadline;

  lp::fixed_deque<cursor_context, MAX_PENDING_CURSOR>    cursor_q;
  lp::fixed_deque<lp_comp_info *, MAX_PENDING_MSG>       unproc_q;
  lp::fixed_deque<KVMFRSetCursorPos, MAX_PENDING_CURSOR> unproc_align;
  lp::fixed_deque<NFRCursorMetadata, MAX_PENDING_CURSOR> unproc_cursor;
};

void common_keep_lgmp_connected(thread_id thr_id);

void common_update_lgmp_state(thread_id thr_id, LGMP_STATUS retcode);

lp_lgmp_msg common_get_lgmp_message(thread_id thr_id);

void common_post_recv(thread_id thr_id);

int common_poll_cq(thread_id thr_id, fi_cq_data_entry * de,
                   fi_cq_err_entry * err);

#endif