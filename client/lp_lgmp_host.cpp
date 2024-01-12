// SPDX-License-Identifier: GPL-2.0-or-later
// Telescope Project
// Looking Glass Proxy (LGProxy)
// Copyright (c) 2022 - 2024, Telescope Project Developers

#include "lp_lgmp_host.hpp"
#include "lp_utils.h"

enum {
  Q_FREE,
  Q_RESERVED,
  Q_PENDING,
  Q_SENT
};

/* --------------------------------------------------------------- */

lp_lgmp_mem::lp_lgmp_mem() {
  q         = 0;
  itm_size  = 0;
  alignment = 0;
  mem.clear();
  states.clear();
}

void lp_lgmp_mem::bind(lp_lgmp_host_q * q) {
  if (this->q) {
    this->clear();
  }
  this->q = q;
}

bool lp_lgmp_mem::alloc(uint32_t size, uint32_t alignment, uint32_t n) {
  assert(size);
  assert(n);
  LGMP_STATUS s;
  for (uint32_t i = 0; i < n; ++i) {
    PLGMPMemory lmem;
    mem_state   state = M_FREE;
    if (alignment)
      s = lgmpHostMemAllocAligned(q->host, size, alignment, &lmem);
    else
      s = lgmpHostMemAlloc(q->host, size, &lmem);
    if (s != LGMP_OK)
      break;
    this->mem.push_back(lmem);
    this->states.push_back(state);
  }
  if (s != LGMP_OK) {
    this->clear();
    return false;
  }
  this->itm_size  = size;
  this->alignment = alignment;
  return true;
}

int lp_lgmp_mem::reserve(int index) {
  if (index < 0) {
    for (uint32_t i = 0; i < states.size(); ++i) {
      if (states[i] == M_FREE) {
        states[i] = M_RESERVED;
        return i;
      }
    }
    return -EAGAIN;
  }
  if (states[index] == M_FREE) {
    states[index] = M_RESERVED;
    return index;
  }
  return -EAGAIN;
}

void lp_lgmp_mem::release(int index) { states[index] = M_FREE; }

void lp_lgmp_mem::clear() {
  this->states.clear();
  for (uint32_t i = 0; i < this->mem.size(); ++i) {
    lgmpHostMemFree(&this->mem[i]);
  }
  this->mem.clear();
}

PLGMPMemory lp_lgmp_mem::operator[](int i) {
  if (i < 0)
    return 0;
  return this->mem.at(i);
}

void lp_lgmp_mem::export_buffers(void ** out) {
  for (uint32_t i = 0; i < this->mem.size(); ++i) {
    out[i] = lgmpHostMemPtr(mem[i]);
  }
}

void lp_lgmp_mem::export_offsets(NFROffset * out) {
  uintptr_t base = (uintptr_t) q->mem->get_ptr();
  for (uint32_t i = 0; i < this->mem.size(); ++i) {
    out[i] = (uintptr_t) lgmpHostMemPtr(mem[i]) - base;
  }
}

uint32_t lp_lgmp_mem::reclaim(PLGMPHostQueue q, uint8_t * out) {
  void *   used[LGMP_Q_POINTER_LEN];
  size_t   n    = lgmpHostQueueMemPending(q, used);
  uint32_t k    = 0;
  bool     flag = false;
  for (uint32_t i = 0; i < this->mem.size(); ++i) {
    for (uint32_t j = 0; j < n; ++j) {
      if (lgmpHostMemPtr(this->mem[i]) == used[j]) {
        flag = true;
        break;
      }
    }
    if (!flag) {
      this->states[i] = M_FREE;
      out[k++]        = i;
    }
  }
  return k;
}

lp_lgmp_mem::~lp_lgmp_mem() {
  this->clear();
  this->q = 0;
}

/* --------------------------------------------------------------- */

void lp_lgmp_host_q::clear_fields() {
  this->host    = 0;
  this->frame_q = 0;
  this->ptr_q   = 0;
  this->mem_frame.clear();
  this->mem_frame.bind(nullptr);
  this->mem_ptr.clear();
  this->mem_ptr.bind(nullptr);
  this->mem_shape.clear();
  this->mem_shape.bind(nullptr);
  this->mem = 0;
}

void lp_lgmp_host_q::release_resources() {
  // Release the host resources if we already have one active
  if (this->host)
    lgmpHostFree(&this->host);
  this->mem_frame.clear();
  this->mem_frame.bind(nullptr);
  this->mem_ptr.clear();
  this->mem_ptr.bind(nullptr);
  this->mem_shape.clear();
  this->mem_shape.bind(nullptr);
  this->frame_q = 0;
  this->ptr_q   = 0;
}

int lp_lgmp_host_q::reset(void * udata, size_t udata_size, bool copy) {
  LGMP_STATUS s;
  this->release_resources();
  s = lgmpHostInit(this->mem->get_ptr(), this->mem->get_size(), &this->host,
                   udata_size, (uint8_t *) udata);
  if (s != LGMP_OK) {
    lp_log_error("Unable to initialize LGMP host: %s", lgmpStatusString(s));
    errno = s;
    return -EIO;
  }

  static const struct LGMPQueueConfig FRAME_QUEUE_CONFIG = {
      .queueID     = LGMP_Q_FRAME,
      .numMessages = LGMP_Q_FRAME_LEN,
      .subTimeout  = 1000};

  static const struct LGMPQueueConfig POINTER_QUEUE_CONFIG = {
      .queueID     = LGMP_Q_POINTER,
      .numMessages = LGMP_Q_POINTER_LEN,
      .subTimeout  = 1000};

  s = lgmpHostQueueNew(this->host, FRAME_QUEUE_CONFIG, &this->frame_q);
  if (s != LGMP_OK) {
    lp_log_error("LGMP frame queue creation failed: %s", lgmpStatusString(s));
    lgmpHostFree(&this->host);
    errno = s;
    return -EIO;
  }
  s = lgmpHostQueueNew(this->host, POINTER_QUEUE_CONFIG, &this->ptr_q);
  if (s != LGMP_OK) {
    lp_log_error("LGMP cursor queue creation failed: %s", lgmpStatusString(s));
    lgmpHostFree(&this->host);
    errno = s;
    return -EIO;
  }

  this->mem_ptr.bind(this);
  if (!this->mem_ptr.alloc(sizeof(KVMFRCursor), 0, LGMP_Q_POINTER_LEN))
    return -ENOBUFS;

  this->mem_shape.bind(this);
  if (!this->mem_shape.alloc(MAX_POINTER_SIZE_ALIGN, tcm_get_page_size(),
                             POINTER_SHAPE_BUFFERS))
    return -ENOBUFS;

  size_t ps   = tcm_get_page_size();
  size_t left = lgmpHostMemAvail(this->host) / LGMP_Q_FRAME_LEN;
  if (!left)
    return -ENOBUFS;

  left -= left % ps;
  lp_log_info("Maximum frame size: %lu", left);

  this->mem_frame.bind(this);
  if (!this->mem_frame.alloc(left, tcm_get_page_size(), LGMP_Q_FRAME_LEN))
    return -ENOBUFS;

  if (copy) {
    void * ud = malloc(udata_size);
    if (!ud)
      throw tcm_exception(ENOMEM, _LP_FNAME_, __LINE__,
                          "Could not allocate user data");
    memcpy(ud, udata, udata_size);
    this->udata.reset(ud);
  } else {
    if (this->udata.get() != udata)
      this->udata.reset(udata);
  }

  return 0;
}

int lp_lgmp_host_q::reset(lp_metadata & mtd, uint32_t feature_flags) {
  int ret;
  errno          = 0;
  size_t ud_size = 0;

  ret = mtd.export_udata(nullptr, &ud_size, feature_flags);
  if (ret != -ENOBUFS) {
    assert(false && "Failed to get expected LGMP metadata buffer size");
    throw tcm_exception(EINVAL, _LP_FNAME_, __LINE__,
                        "Failed to get expected LGMP metadata buffer size");
  }
  void * ud = malloc(ud_size);
  if (!ud)
    throw tcm_exception(ENOMEM, _LP_FNAME_, __LINE__,
                        "Failed to allocate LGMP metadata buffer");
  ret = mtd.export_udata(ud, &ud_size, feature_flags);
  if (ret < 0) {
    assert(false && "Failed to convert LGMP metadata");
    throw tcm_exception(EINVAL, _LP_FNAME_, __LINE__,
                        "Failed to convert LGMP metadata");
  }

  ret = this->reset(ud, ud_size, false);
  if (ret < 0)
    free(ud);
  return ret;
}

lp_lgmp_host_q::lp_lgmp_host_q(std::shared_ptr<lp_shmem> & mem) {
  this->clear_fields();
  this->mem = mem;
}

lp_lgmp_host_q::~lp_lgmp_host_q() {
  this->release_resources();
  this->udata.reset();
  this->mem = 0;
}

int lp_lgmp_host_q::process(void * out, size_t * size) {
  if (*size < LGMP_MSGS_SIZE) {
    *size = LGMP_MSGS_SIZE;
    return -ENOBUFS;
  }
  LGMP_STATUS ret = lgmpHostProcess(this->host);
  switch (ret) {
    case LGMP_OK: break;
    case LGMP_ERR_CORRUPTED: {
      int res = this->reset(this->udata.get(), this->udata_size, false);
      if (res < 0)
        return res;
      break;
    }
    default:
      lp_log_error("LGMP error: %s", lgmpStatusString(ret));
      throw tcm_exception(ECOMM, _LP_FNAME_, __LINE__, "LGMP protocol error");
  }
  ret = lgmpHostReadData(this->ptr_q, out, size);
  switch (ret) {
    case LGMP_OK: return 1;
    case LGMP_ERR_QUEUE_EMPTY: return 0;
    default:
      lp_log_error("LGMP error: %s", lgmpStatusString(ret));
      throw tcm_exception(ECOMM, _LP_FNAME_, __LINE__, "LGMP protocol error");
  }
}

LGMP_STATUS lp_lgmp_host_q::post_cursor(PLGMPMemory m, uint32_t udata) {
  return lgmpHostQueuePost(this->ptr_q, udata, m);
}

LGMP_STATUS lp_lgmp_host_q::post_frame(PLGMPMemory m, uint32_t udata) {
  return lgmpHostQueuePost(this->frame_q, udata, m);
}

int lp_lgmp_host_q::num_clients(uint32_t q_id) {
  uint32_t     ids[32];
  unsigned int count = 0;
  LGMP_STATUS  s;
  switch (q_id) {
    case LGMP_Q_POINTER:
      s = lgmpHostGetClientIDs(this->ptr_q, ids, &count);
      break;
    case LGMP_Q_FRAME:
      s = lgmpHostGetClientIDs(this->frame_q, ids, &count);
      break;
    default:
      throw tcm_exception(EINVAL, "Invalid queue ID");
  }
  if (s == LGMP_OK)
    return (int) count;
  return -(int) s;
}