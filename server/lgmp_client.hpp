/*
    SPDX-License-Identifier: GPL-2.0-or-later

    Telescope Project
    Looking Glass Proxy

    Copyright (c) 2022 - 2023 Telescope Project Developers

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
    more details.

    You should have received a copy of the GNU General Public License along with
    this program; if not, write to the Free Software Foundation, Inc., 51
    Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#ifndef LP_LGMP_CLIENT_HPP_
#define LP_LGMP_CLIENT_HPP_

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "lp_metadata.h"

#include <atomic>
typedef std::atomic_uint_least32_t atomic_uint_least32_t;

#if defined(__GNUC__)
#define restrict __restrict__
#elif defined(__clang__)
#define restrict __restrict__
#else
#define restrict
#endif

extern "C" {
#include "common/KVMFR.h"
#include "common/framebuffer.h"
#include "lgmp/client.h"
}

#include "lp_log.h"
#include "lp_mem.h"
#include "lp_time.h"
#include "lp_types.h"
#include "lp_utils.h"

#include "tcm_time.h"

#include "nfr_vmsg.hpp"

using std::shared_ptr;

#include "lp_version.h"
#if LP_KVMFR_SUPPORTED_VER != KVMFR_VERSION
#error KVMFR version mismatch!
#endif

/* Add KVMFR user data to NetFR host metadata message */
void lp_kvmfr_udata_to_nfr(void * udata, size_t udata_size,
                           NFRHostMetadataConstructor & c);

struct lp_lgmp_msg {
  /* Pointer to the message. */
  void *   ptr;
  /* Size of the message. If an error occurred, this value is negative and
     the error is a value in the LGMP_STATUS enum, negated. */
  ssize_t  size;
  /* User data (flags) associated with the LGMP message. */
  uint32_t udata;

  KVMFRFrame *  frame() { return reinterpret_cast<KVMFRFrame *>(this->ptr); }
  KVMFRCursor * cursor() { return reinterpret_cast<KVMFRCursor *>(this->ptr); }

  void import(PLGMPMessage msg) {
    size  = msg->size;
    udata = msg->udata;
    ptr   = msg->mem;
  }

  void export_frame(KVMFRFrame * out, bool damage, bool texture) {
    if (!this->ptr) {
      assert(false && "Message empty!");
      throw tcm_exception(EINVAL, __FILE__, __LINE__, "Message empty!");
    }
    if (size < (ssize_t) sizeof(KVMFRFrame))
      throw tcm_exception(EINVAL, __FILE__, __LINE__,
                          "Message functions called on invalid data");
    if (texture)
      damage = 1;
    size_t n = damage ? sizeof(KVMFRFrame)
                      : sizeof(KVMFRFrame) - sizeof(out->damageRects);
    n += texture ? this->frame()->offset + FB_WP_SIZE +
                       this->frame()->pitch * this->frame()->dataHeight
                 : 0;
    memcpy((void *) out, this->ptr, n);
    if (!damage)
      out->damageRectsCount = 0;
  }

  void export_cursor(KVMFRCursor * out, bool texture) {
    if (!this->ptr) {
      assert(false && "Message empty!");
      throw tcm_exception(EINVAL, __FILE__, __LINE__, "Message empty!");
    }
    if (size < (ssize_t) sizeof(KVMFRCursor))
      throw tcm_exception(EINVAL, __FILE__, __LINE__,
                          "Message functions called on invalid data");
    size_t n = texture ? sizeof(KVMFRCursor) +
                             this->cursor()->pitch * this->cursor()->height
                       : sizeof(KVMFRCursor);
    memcpy((void *) out, this->ptr, n);
  }
};

class lp_lgmp_client_q {

  void clear_fields();

public:
  bool     msg_pending;
  bool     allow_skip;
  void *   udata;
  uint32_t udata_size;
  uint32_t client_id;
  uint32_t q_id;
  timespec last_msg;

  PLGMPClient      lgmp;
  PLGMPClientQueue q;

  lp_metadata mtd;
  int64_t     timeout_ms;
  int64_t     interval_us;

  volatile int * exit_flag;

  std::shared_ptr<lp_shmem> mem;

  lp_lgmp_client_q(std::shared_ptr<lp_shmem> mem, int64_t timeout_ms = 1000,
                   int64_t interval_us = 100);
  ~lp_lgmp_client_q();

  void bind_exit_flag(volatile int * flag);

  /* Initialize the LGMP session. */
  LGMP_STATUS      init();
  /* Subscribe to a queue with the specified ID. */
  LGMP_STATUS      subscribe(uint32_t q_id);
  /* Unsubscribe from a queue. */
  LGMP_STATUS      unsubscribe();
  /* Get a new message from the queue. */
  lp_lgmp_msg      get_msg();
  /* Send a message to the LGMP host. */
  LGMP_STATUS      send_msg(const void * data, size_t size, uint32_t * serial);
  /* Acknowledge a pending message. */
  LGMP_STATUS      ack_msg();
  /* Combination of init() + subscribe(). */
  LGMP_STATUS      connect(uint32_t q_id);
  /* Get LGMP host metadata. */
  lp_metadata &    get_metadata();
  /* Get session status (not subscription status). */
  bool             connected();
  /* Get the timestamp of when the last message was received. */
  const timespec & last_msg_time();
};

#endif