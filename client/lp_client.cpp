#include "lg_build_version.h"
#include "lp_build_version.h"
#include "lp_config.h"
#include "lp_exception.h"
#include "lp_lgmp_host.hpp"
#include "lp_metadata.h"
#include "lp_queue.h"
#include "lp_state.h"
#include "lp_time.h"
#include "lp_utils.h"

#include "nfr_client.hpp"
#include "nfr_protocol.h"
#include "nfr_util.hpp"
#include "nfr_vmsg.hpp"

#include "tcm_fabric.h"

#include <atomic>
#include <memory>
#include <unordered_set>

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>

#define MSG_BUF_SIZE 1048576
#define MAX_MSG_SIZE 1024
#define MAX_FRAME_MSG_SIZE 1024
#define MAX_QUEUED_MSGS 256
#define MAX_PENDING_FRAME 2
#define MAX_PENDING_CURSOR (LGMP_Q_POINTER_LEN + POINTER_SHAPE_BUFFERS)

using std::atomic_uint_least32_t;

enum session_id {
  SESSION_FRAME  = 1,
  SESSION_CURSOR = 2
};

enum system_flags : uint32_t {

  // Active
  S_ACTIVE = (1 << 0), // Relay ready and active

  // Initialization
  S_INIT = (1 << 1),

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

extern "C" {
#include "common/KVMFR.h"
#include "common/framebuffer.h"
}

#define MSG_CT_SIZE LGMP_MSGS_SIZE

using std::FB_WP_TYPE;
using std::make_shared;
using std::shared_ptr;
using std::unique_ptr;

volatile int exit_flag = 0;

void exit_handler(int sig) {
  lp_log_info("Signal %d", sig);
  if (sig == SIGINT) {
    lp_log_info("Ctrl+C %d of 3, press thrice to force quit", ++exit_flag);
    if (exit_flag >= 3) {
      lp_log_info("Force quitting");
      exit(EINTR);
    }
  }
}

struct msg_container {
  char     msg[MSG_CT_SIZE];
  uint32_t udata;
  int8_t   index;
};

enum buffer_tag : uint8_t {
  TAG_INVALID,
  TAG_MSG_RX,
  TAG_MSG_TX,
  TAG_FRAME_TX,
  TAG_FRAME_RX,
  TAG_MAX
};

static inline const char * get_buffer_tag_str(uint8_t tag) {
  switch (tag) {
    case TAG_MSG_RX: return "MSG_RX";
    case TAG_MSG_TX: return "MSG_TX";
    case TAG_FRAME_RX: return "FRAME_RX";
    case TAG_FRAME_TX: return "FRAME_TX";
    default: return "?";
  }
}

struct t_main_state {
  int64_t                                                lgmp_timeout;
  int64_t                                                lgmp_interval;
  int64_t                                                net_timeout;
  int64_t                                                net_interval;
  unique_ptr<lp_rdma_shmem>                              rshm;
  NFRClientResource *                                    resrc;
  shared_ptr<lp_mbuf>                                    mbuf;
  unique_ptr<lp_lgmp_host_q>                             lgmp;
  shared_ptr<lp_sink_opts>                               opts;
  uint64_t                                               page_size;
  lp::fixed_deque<NFRFrameMetadata, MAX_PENDING_FRAME>   pending_frame;
  lp::fixed_deque<NFRCursorMetadata, MAX_PENDING_CURSOR> pending_cursor;
  lp::fixed_deque<msg_container, MAX_PENDING_CURSOR>     lgmp_cur_tx;
  lp::buffer_sync                                        frame_sync;
  lp::buffer_sync                                        cursor_sync;
  uint32_t                                               frame_counter;
  ssize_t                                                ret;
  shared_ptr<tcm_cq>                                     msg_cq;
  shared_ptr<tcm_cq>                                     frame_cq;
  lp_metadata                                            mtd;
  uint32_t                                               ctrl;
  timespec                                               net_deadline;
  timespec                                               lgmp_deadline;
};

t_main_state t_state;

// Receive metadata from the NetFR server and setup the LGMP host, sending the
// created LGMP buffers back to the NetFR server
static ssize_t mt_initial_setup() {

  // Allocate message buffers
  NFRClientResource * nfr = t_state.resrc;
  t_state.mbuf.reset(new lp_mbuf(nfr->fabric, MSG_BUF_SIZE));
  tcm_mem * mem = t_state.mbuf->get_mem();

  // Receive initial metadata message. This can be larger than any other
  // messages sent during the session, so allow the entire buffer to be used.

  ssize_t ret;
  lp_log_info("Waiting for server metadata");
  ret = nfr->ep_msg->srecv(*mem, nfr->peer_msg, 0, MSG_BUF_SIZE);
  if (ret < 0)
    LP_THROW(-ret, "Metadata receive failed");
  if (!nfrHeaderVerify(**mem, ret))
    LP_THROW(EPROTO, "Invalid header data");
  t_state.mtd.import_nfr(static_cast<NFRHostMetadata *>(**mem), ret);

  // Set up own buffers

  lp_log_debug("Resetting LGMP queue");
  t_state.lgmp.reset(new lp_lgmp_host_q(t_state.rshm->shm));
  ret = t_state.lgmp->reset(t_state.mtd, (unsigned int) 0);
  if (ret < 0) {
    lp_log_error("Failed to reset LGMP session: %s",
                 lgmpStatusString((LGMP_STATUS) errno));
    return ret;
  }

  if (!t_state.mbuf->alloc(TAG_MSG_RX, MAX_MSG_SIZE, 32))
    LP_THROW(ENOMEM, "Buffer allocation failed");
  if (!t_state.mbuf->alloc(TAG_MSG_TX, MAX_MSG_SIZE, 32))
    LP_THROW(ENOMEM, "Buffer allocation failed");
  if (!t_state.mbuf->alloc(TAG_FRAME_RX, MAX_MSG_SIZE, 4))
    LP_THROW(ENOMEM, "Buffer allocation failed");
  if (!t_state.mbuf->alloc(TAG_FRAME_TX, MAX_MSG_SIZE, 4))
    LP_THROW(ENOMEM, "Buffer allocation failed");
  lp_log_debug("Allocated buffers");

  // Collect allocated buffers

  void * fb_base = ((uint8_t *) **mem) + MAX_MSG_SIZE;

  NFRClientCursorBuf * cb = static_cast<NFRClientCursorBuf *>(**mem);
  cb->_pad[0]             = 0;
  cb->_pad[1]             = 0;
  cb->_pad[2]             = 0;
  cb->header              = NFRHeaderCreate(NFR_MSG_CLIENT_CURSOR_BUF);
  cb->base                = (uintptr_t) t_state.rshm->mem->get_ptr();
  cb->maxlen              = t_state.lgmp->mem_shape.itm_size;
  t_state.rshm->mem->get_rkey(&cb->key);
  size_t cb_size = sizeof(*cb);
  for (uint32_t i = 0; i < t_state.lgmp->mem_shape.mem.size(); ++i) {
    void * buf = lgmpHostMemPtr(t_state.lgmp->mem_shape[i]);
    if (!buf)
      LP_THROW(EINVAL, "Buffer error");
    cb->offsets[i] = (uintptr_t) buf - (uintptr_t) (**t_state.lgmp->mem) +
                     sizeof(KVMFRCursor);
    cb_size += sizeof(*cb->offsets);
  }

  NFRClientFrameBuf * fb = static_cast<NFRClientFrameBuf *>(fb_base);
  fb->_pad[0]            = 0;
  fb->_pad[1]            = 0;
  fb->_pad[2]            = 0;
  fb->header             = NFRHeaderCreate(NFR_MSG_CLIENT_FRAME_BUF);
  fb->base               = (uintptr_t) t_state.rshm->mem->get_ptr();
  fb->maxlen             = t_state.lgmp->mem_frame.itm_size;
  t_state.rshm->mem->get_rkey(&fb->key);
  size_t fb_size = sizeof(*fb);
  for (uint32_t i = 0; i < t_state.lgmp->mem_frame.mem.size(); ++i) {
    void * buf = lgmpHostMemPtr(t_state.lgmp->mem_frame[i]);
    if (!buf)
      LP_THROW(EINVAL, "Buffer error");
    fb->offsets[i] =
        (uintptr_t) buf - (uintptr_t) (**t_state.lgmp->mem) + t_state.page_size;
    fb_size += sizeof(*fb->offsets);
  }

  lp_log_trace("Sending cursor buffer information");
  ret = nfr->ep_msg->send(*mem, nfr->peer_msg, (void *) 1, 0, cb_size);
  if (ret < 0) {
    lp_log_error("Failed to send cursor buffer info");
    return ret;
  }

  lp_log_trace("Sending frame buffer information");
  ret = nfr->ep_frame->send(*mem, nfr->peer_frame, (void *) 2, MAX_MSG_SIZE,
                            fb_size);
  if (ret < 0) {
    lp_log_error("Failed to send frame buffer info");
    return ret;
  }

  lp_log_trace("Waiting for send completion");
  timespec dl      = lp_get_deadline(t_state.net_timeout);
  tcm_cq * cq_m    = nfr->ep_msg->get_cq().lock().get();
  tcm_cq * cq_f    = nfr->ep_frame->get_cq().lock().get();
  uint8_t  waiting = 2;
  do {
    fi_cq_data_entry de;
    fi_cq_err_entry  err;
    ret = cq_m->poll(&de, &err, 1, nullptr, 0);
    if (ret < 0 && ret != -EAGAIN) {
      if (ret == -FI_EAVAIL)
        lp_log_error("Fabric poll error: %s", fi_strerror(err.err));
      else
        lp_log_error("Fabric poll error: %s", fi_strerror(-ret));
      break;
    }
    if (ret == 1)
      waiting--;
    ret = cq_f->poll(&de, &err, 1, nullptr, 0);
    if (ret < 0 && ret != -EAGAIN) {
      if (ret == -FI_EAVAIL)
        lp_log_error("Fabric poll error: %s", fi_strerror(err.err));
      else
        lp_log_error("Fabric poll error: %s", fi_strerror(-ret));
      break;
    }
    if (ret == 1)
      waiting--;
  } while (waiting && !exit_flag && !lp_check_deadline(dl));

  if (exit_flag || lp_check_deadline(dl))
    LP_THROW(ETIMEDOUT, "Exiting");

  if (waiting)
    LP_THROW(ETIMEDOUT, "Connection setup failed");

  lp_log_debug("Initial setup complete");
  return 0;
}

// Fill receive slots of both frame and general message queue
static ssize_t mt_post_recvs() {
  NFRClientResource * nfr = t_state.resrc;
  ssize_t             ret;
  ssize_t             nf = 0, nc = 0;
  buffer_tag          tag = TAG_FRAME_RX;
  tcm_mem *           mem = t_state.mbuf->get_mem();
  int                 idx = -1;

  while ((idx = t_state.mbuf->lock(tag)) >= 0) {
    lp_comp_info * ctx = t_state.mbuf->get_context(tag, idx);
    uint64_t       off = t_state.mbuf->get_offset(tag, idx);
    ret = nfr->ep_frame->recv(*mem, nfr->peer_frame, ctx->fctx, off,
                              MAX_FRAME_MSG_SIZE);
    if (ret == -FI_EAGAIN)
      break;
    if (ret < 0) {
      lp_log_debug("Frame metadata recv post error: %s", fi_strerror(-ret));
      return ret;
    }
    nf++;
  }

  tag = TAG_MSG_RX;
  while ((idx = t_state.mbuf->lock(tag)) >= 0) {
    lp_comp_info * ctx = t_state.mbuf->get_context(tag, idx);
    uint64_t       off = t_state.mbuf->get_offset(tag, idx);
    ret = nfr->ep_msg->recv(*mem, nfr->peer_msg, ctx->fctx, off, MAX_MSG_SIZE);
    if (ret == -FI_EAGAIN)
      break;
    if (ret < 0) {
      lp_log_debug("Message recv post error: %s", fi_strerror(-ret));
      return ret;
    }
    nc++;
  }

  if (nf || nc)
    lp_log_trace("Receives posted: %d frame, %d cursor", nf, nc);
  return nf + nc;
}

// Send the buffers reclaimed in mt_reclaim_lgmp_bufs() to the NetFR server
static ssize_t mt_sync_remote_buffers() {

  // If paused, we can update the freed buffers when we resume the session
  if (t_state.ctrl &
      (S_C_PAUSE_LOCAL_SYNC | S_C_PAUSE_LOCAL_UNSYNC | S_C_PAUSE_REMOTE)) {
    return 0;
  }

  NFRClientResource * nfr = t_state.resrc;
  tcm_mem *           mem = t_state.mbuf->get_mem();
  buffer_tag          tag = TAG_MSG_TX;
  int                 idx;
  ssize_t             ret, ack_size;

  uint8_t n;
  uint8_t indexes[MAX_PENDING_CURSOR];

  n = t_state.cursor_sync.get_unsynced(indexes);
  if (n) {
    idx = t_state.mbuf->lock(tag);
    if (idx >= 0) {
      lp_comp_info * info = t_state.mbuf->get_context(tag, idx);
      uint64_t       off  = t_state.mbuf->get_offset(tag, idx);
      NFRClientAck * ack  = (NFRClientAck *) t_state.mbuf->get_buffer(tag, idx);
      ack->header         = NFRHeaderCreate(NFR_MSG_CLIENT_ACK);
      ack->type           = NFR_BUF_CURSOR_DATA;
      ack_size            = sizeof(*ack) + sizeof(*ack->indexes) * n;
      for (uint8_t i = 0; i < n; ++i) {
        ack->indexes[i] = indexes[i];
      }
      ret = nfr->ep_msg->send(*mem, nfr->peer_msg, info->fctx, off, ack_size);
      if (ret < 0) {
        t_state.mbuf->unlock(tag, idx);
        if (ret != -FI_EAGAIN) {
          lp_log_error("Could not send cursor buffer sync message: %s",
                       fi_strerror(-ret));
          return ret;
        }
      }
      t_state.cursor_sync.set_synced(indexes, n);
    }
  }

  tag = TAG_FRAME_TX;
  n   = t_state.frame_sync.get_unsynced(indexes);
  if (n) {
    idx = t_state.mbuf->lock(tag);
    if (idx >= 0) {
      lp_comp_info * info = t_state.mbuf->get_context(tag, idx);
      uint64_t       off  = t_state.mbuf->get_offset(tag, idx);
      NFRClientAck * ack  = (NFRClientAck *) t_state.mbuf->get_buffer(tag, idx);
      ack->header         = NFRHeaderCreate(NFR_MSG_CLIENT_ACK);
      ack->type           = NFR_BUF_FRAME;
      ack_size            = sizeof(*ack) + sizeof(*ack->indexes) * n;
      for (uint8_t i = 0; i < n; ++i) {
        ack->indexes[i] = indexes[i];
      }
      ret =
          nfr->ep_frame->send(*mem, nfr->peer_frame, info->fctx, off, ack_size);
      if (ret < 0) {
        t_state.mbuf->unlock(tag, idx);
        if (ret != -FI_EAGAIN) {
          lp_log_error("Could not send frame buffer sync message: %s",
                       fi_strerror(-ret));
          return ret;
        }
      }
      t_state.frame_sync.set_synced(indexes, n);
    }
  }

  return 0;
}

// Reclaim any LGMP buffers that have been acknowledged by the LGMP client
static ssize_t mt_get_unused_bufs() {
  uint32_t n;
  uint8_t  indexes[LGMP_Q_POINTER_LEN];
  n = t_state.lgmp->mem_shape.reclaim(t_state.lgmp->ptr_q, indexes);
  t_state.cursor_sync.set_freed(indexes, n);
  n = t_state.lgmp->mem_frame.reclaim(t_state.lgmp->frame_q, indexes);
  t_state.frame_sync.set_freed(indexes, n);
  return 0;
}

// Update system state based on incoming message
static ssize_t mt_process_in_state(NFRState * st, ssize_t size, session_id id) {
  assert(st);
  if ((size_t) size < sizeof(*st))
    return -EBADMSG;

  switch (st->state) {
    case NFR_STATE_KA:
      t_state.net_deadline = lp_get_deadline(t_state.net_timeout);
      return 0;
    case NFR_STATE_PAUSE:
      switch (id) {
        case SESSION_CURSOR: t_state.ctrl |= S_C_PAUSE_REMOTE; return 0;
        case SESSION_FRAME: t_state.ctrl |= S_F_PAUSE_REMOTE; return 0;
        default: LP_THROW(EINVAL, "Invalid session ID");
      }
    case NFR_STATE_DISCONNECT: throw lp_exit(LP_EXIT_REMOTE);
    default: LP_THROW(EPROTO, "Communication error");
  }
}

// Post an LGMP frame
static ssize_t mt_post_lgmp_frame(NFRFrameMetadata * mtd) {
  LGMP_STATUS s;
  if (mtd->buffer >= 0) {
    if (t_state.lgmp->mem_frame.reserve(mtd->buffer) < 0) {
      lp_log_warn("Write occurred to buffer already in use");
    }
    PLGMPMemory mem = t_state.lgmp->mem_frame[mtd->buffer];
    if (!mem) {
      lp_log_warn("Server sent invalid frame metadata");
      return -EBADMSG;
    }
    void * buf = lgmpHostMemPtr(mem);
    if (!buf) {
      assert(false && "Invalid state");
      LP_THROW(EINVAL, "Invalid buffer");
    }
    KVMFRFrame * f   = static_cast<KVMFRFrame *>(buf);
    f->formatVer     = 0;
    f->frameSerial   = t_state.frame_counter++;
    f->type          = (FrameType) mtd->frame_type;
    f->screenWidth   = mtd->width;
    f->screenHeight  = mtd->height;
    f->dataWidth     = mtd->width;
    f->dataHeight    = mtd->height;
    f->frameWidth    = mtd->width;
    f->frameHeight   = mtd->height;
    f->rotation      = (FrameRotation) mtd->frame_rotation;
    f->stride        = mtd->width;
    f->pitch         = mtd->row_bytes;
    f->offset        = t_state.page_size - FB_WP_SIZE;
    FrameBuffer * fb = (FrameBuffer *) ((uint8_t *) buf + f->offset);
    framebuffer_set_write_ptr(fb, mtd->row_bytes * mtd->height);

    s = t_state.lgmp->post_frame(mem, mtd->flags);
    if (s == LGMP_OK)
      return 0;
    return -EAGAIN;
  } else {
    return -ERESTART;
  }
}

// Post an LGMP cursor, with or without metadata
static ssize_t mt_post_lgmp_cursor(NFRCursorMetadata * mtd) {

  lp_log_trace("Posting LGMP cursor");
  if (mtd->buffer >= 0) {

    // Cursor shape data was also included
    PLGMPMemory mem = t_state.lgmp->mem_shape[mtd->buffer];
    if (!mem) {
      lp_log_error("Invalid message received!");
      return -EBADMSG;
    }
    if (t_state.lgmp->mem_shape.reserve(mtd->buffer) < 0) {
      lp_log_error("Buffer already in use!");
      return -ENOBUFS;
    }
    KVMFRCursor * cur = (KVMFRCursor *) lgmpHostMemPtr(mem);
    assert(cur);
    cur->x         = mtd->x;
    cur->y         = mtd->y;
    cur->hx        = mtd->hx;
    cur->hy        = mtd->hy;
    cur->width     = mtd->width;
    cur->height    = mtd->height;
    cur->pitch     = mtd->row_bytes;
    uint32_t flags = mtd->flags | CURSOR_FLAG_SHAPE;
    if (t_state.lgmp->post_cursor(mem, flags) != LGMP_OK) {
      t_state.lgmp->mem_shape.release(mtd->buffer);
      return -EAGAIN;
    }
    return 0;

  } else {

    // Only cursor metadata
    int idx = t_state.lgmp->mem_ptr.reserve();
    if (idx < 0)
      return 0;
    PLGMPMemory mem = t_state.lgmp->mem_ptr[idx];
    if (!mem)
      return 0;
    KVMFRCursor * cur = (KVMFRCursor *) lgmpHostMemPtr(mem);
    assert(cur);
    cur->x         = mtd->x;
    cur->y         = mtd->y;
    cur->hx        = mtd->hx;
    cur->hy        = mtd->hy;
    cur->width     = 0;
    cur->height    = 0;
    cur->pitch     = 0;
    uint32_t flags = mtd->flags & ~CURSOR_FLAG_SHAPE;
    if (t_state.lgmp->post_cursor(mem, flags) != LGMP_OK) {
      t_state.lgmp->mem_ptr.release(idx);
      return -EAGAIN;
    }
    return 0;
  }
}

// Try to post the cursor data immediately. If it failed, enqueue it
static ssize_t mt_process_in_cursor(NFRCursorMetadata * mtd, ssize_t size) {
  assert(mtd);
  if ((size_t) size < sizeof(*mtd))
    return -EBADMSG;

  ssize_t ret = mt_post_lgmp_cursor(mtd);
  if (ret == -EAGAIN)
    t_state.pending_cursor.push(*mtd);
  return ret;
}

// Try to post the frame immediately. If it failed, enqueue it
static ssize_t mt_process_in_frame(NFRFrameMetadata * mtd, ssize_t size) {
  assert(mtd);
  if ((size_t) size < sizeof(*mtd))
    return -EINVAL;

  ssize_t ret = mt_post_lgmp_frame(mtd);
  if (ret == -EAGAIN) {
    // If the LGMP queue is full we store the message in our local queue
    t_state.pending_frame.push(*mtd);
  }
  return ret;
}

// Input network message processing
static ssize_t mt_process_in(fi_cq_data_entry * de) {
  lp_comp_info * info = t_state.mbuf->get_context(de->op_context);
  void *         buf  = t_state.mbuf->get_buffer(info);
  lp_log_trace("buftag: %s (%d), index: %d, ptr: %p, off: %lu",
               get_buffer_tag_str(info->_buftag), info->_buftag, info->_index,
               buf, t_state.mbuf->get_offset(info));
  if (!nfrHeaderVerify(buf, de->len))
    LP_THROW(EPROTO, "Invalid header data");
  NFRHeader * hdr = static_cast<NFRHeader *>(buf);
  switch (hdr->type) {

    case NFR_MSG_FRAME_METADATA:
      lp_log_trace("Message type: Frame");
      return mt_process_in_frame(static_cast<NFRFrameMetadata *>(buf), de->len);

    case NFR_MSG_CURSOR_METADATA:
      lp_log_trace("Message type: Cursor Metadata");
      return mt_process_in_cursor(static_cast<NFRCursorMetadata *>(buf),
                                  de->len);

    case NFR_MSG_STATE:
      lp_log_trace("Message type: System State");
      if (info->_buftag == TAG_FRAME_RX) {
        return mt_process_in_state(static_cast<NFRState *>(buf), de->len,
                                   SESSION_FRAME);
      } else if (info->_buftag == TAG_MSG_RX) {
        return mt_process_in_state(static_cast<NFRState *>(buf), de->len,
                                   SESSION_CURSOR);
      }
      LP_THROW(EINVAL, "Invalid state");

    default:
      lp_log_error("Invalid message type %d!");
      LP_THROW(EPROTO, "Communication error");
  }
}

static ssize_t mt_process_comp(fi_cq_data_entry * de) {
  lp_log_trace("op ctx: %p", de->op_context);
  lp_comp_info * info = t_state.mbuf->get_context(de->op_context);
  lp_log_trace("Comp buftag: %s, index: %d", get_buffer_tag_str(info->_buftag),
               info->_index);
  switch (info->_buftag) {
    case TAG_MSG_RX:
    case TAG_FRAME_RX: return mt_process_in(de);
    case TAG_MSG_TX:
    case TAG_FRAME_TX: t_state.mbuf->unlock(info); return 0;
    default: LP_THROW(EINVAL, "Invalid tag");
  }
}

// Process messages from the LGMP client and enqueue them into our send queue
static ssize_t mt_process_lgmp_in(void * buf, size_t size) {
  assert(buf);

  // Simply discard the message when paused
  if (t_state.ctrl &
      (S_C_PAUSE_LOCAL_SYNC | S_C_PAUSE_LOCAL_UNSYNC | S_C_PAUSE_REMOTE)) {
    return 0;
  }

  if ((size_t) size < sizeof(KVMFRMessage))
    return -EBADMSG;

  KVMFRMessage * msg = static_cast<KVMFRMessage *>(buf);
  switch (msg->type) {
    case KVMFR_MESSAGE_SETCURSORPOS: {
      msg_container ct;
      memcpy((void *) ct.msg, buf, sizeof(KVMFRSetCursorPos));
      t_state.lgmp_cur_tx.push(ct);
      return 0;
    }
    default: return -EBADMSG;
  }
}

// Process queued cursor messages received from the network and send it to the
// LGMP client. Returns # of processed messages or negative error code
static ssize_t mt_flush_lgmp_pending_cursor_in() {

  // No LGMP clients, discard the cursor data. If the remote side is paused
  // then we can just forward the data we were already sent until the queue
  // is empty
  if (t_state.ctrl & (S_C_PAUSE_LOCAL_SYNC | S_C_PAUSE_LOCAL_UNSYNC)) {
    t_state.pending_cursor.clear();
    return 0;
  }

  uint32_t n = 0;
  while (!t_state.pending_cursor.empty()) {
    NFRCursorMetadata & mtd = t_state.pending_cursor.front();
    ssize_t             ret = mt_post_lgmp_cursor(&mtd);
    if (ret == 0) {
      n++;
      t_state.pending_cursor.pop_front();
      continue;
    }
    break;
  }
  return n;
}

// Flush any queued LGMP cursor messages waiting to be sent over the network
static ssize_t mt_flush_lgmp_pending_cursor_out() {

  // If paused, discard outgoing cursor data
  if (t_state.ctrl &
      (S_C_PAUSE_LOCAL_SYNC | S_C_PAUSE_LOCAL_UNSYNC | S_C_PAUSE_REMOTE)) {
    t_state.lgmp_cur_tx.clear();
    return 0;
  }

  ssize_t             ret;
  NFRClientResource * nfr = t_state.resrc;
  int                 n   = 0;
  tcm_mem *           mem = t_state.mbuf->get_mem();

  while (!t_state.lgmp_cur_tx.empty()) {
    msg_container & msg = t_state.lgmp_cur_tx.front();
    KVMFRMessage *  km  = (KVMFRMessage *) msg.msg;
    switch (km->type) {
      case KVMFR_MESSAGE_SETCURSORPOS: {
        int idx = t_state.mbuf->lock(TAG_MSG_TX);
        if (idx) {
          KVMFRSetCursorPos * scp = (KVMFRSetCursorPos *) msg.msg;
          NFRCursorAlign *    align =
              (NFRCursorAlign *) t_state.mbuf->get_buffer(TAG_MSG_TX, idx);
          lp_comp_info * ctx = t_state.mbuf->get_context(TAG_MSG_TX, idx);
          uint64_t       off = t_state.mbuf->get_offset(TAG_MSG_TX, idx);
          align->header      = NFRHeaderCreate(NFR_MSG_CURSOR_ALIGN);
          align->x           = scp->x;
          align->y           = scp->y;
          lp_log_trace("Send cursor align %d", ctx->_buftag);
          ret = nfr->ep_msg->send(*mem, nfr->peer_msg, ctx->fctx, off,
                                  sizeof(*align));
          if (ret < 0) {
            if (ret == -FI_EAGAIN)
              return n;
            lp_log_error("Failed to send message: %s", fi_strerror(-ret));
            return ret;
          }
          n++;
        } else {
          return n;
        }
        break;
      }
      default:
        // Drop any bad messages from misbehaving clients
        lp_log_warn("Unknown LGMP message type %d", km->type);
    }
    t_state.lgmp_cur_tx.pop_front();
  }
  return n;
}

// Flush any queued LGMP frame messages
static ssize_t mt_flush_lgmp_pending_frame_in() {
  uint32_t n = 0;

  // No LGMP clients, discard the frame data. If the remote side is paused
  // then we can just forward the data we were already sent until the queue
  // is empty
  if (t_state.ctrl & (S_F_PAUSE_LOCAL_SYNC | S_F_PAUSE_LOCAL_UNSYNC)) {
    t_state.pending_frame.clear();
    return 0;
  }

  while (!t_state.pending_frame.empty()) {
    NFRFrameMetadata & mtd = t_state.pending_frame.front();
    ssize_t            ret = mt_post_lgmp_frame(&mtd);
    if (ret == 0) {
      n++;
      t_state.pending_frame.pop_front();
      continue;
    }
    break;
  }
  return n;
}

static ssize_t mt_send_state(session_id id, uint8_t state) {
  NFRClientResource * nfr = t_state.resrc;
  if (id == SESSION_FRAME) {
    int idx = t_state.mbuf->lock(TAG_FRAME_TX);
    if (idx < 0)
      return -EAGAIN;
    lp_comp_info * info = t_state.mbuf->get_context(TAG_FRAME_TX, idx);
    void *         buf  = t_state.mbuf->get_buffer(info);
    uint64_t       off  = t_state.mbuf->get_offset(info);
    NFRState *     st   = static_cast<NFRState *>(buf);
    st->header          = NFRHeaderCreate(NFR_MSG_STATE);
    st->state           = state;
    return nfr->ep_frame->send(*t_state.mbuf->get_mem(), nfr->peer_frame,
                               info->fctx, off, sizeof(*st));
  }
  if (id == SESSION_CURSOR) {
    int idx = t_state.mbuf->lock(TAG_MSG_TX);
    if (idx < 0)
      return -EAGAIN;
    lp_comp_info * info = t_state.mbuf->get_context(TAG_MSG_TX, idx);
    void *         buf  = t_state.mbuf->get_buffer(info);
    uint64_t       off  = t_state.mbuf->get_offset(info);
    NFRState *     st   = static_cast<NFRState *>(buf);
    st->header          = NFRHeaderCreate(NFR_MSG_STATE);
    st->state           = state;
    return nfr->ep_msg->send(*t_state.mbuf->get_mem(), nfr->peer_msg,
                             info->fctx, off, sizeof(*st));
  }
  LP_THROW(EINVAL, "Invalid session ID");
}

// Main "thread", but in this version frame and cursor threads aren't split
void * main_thread(void * arg) {
  (void) arg;
  NFRClientResource * nfr = t_state.resrc;
  ssize_t             ret;

  try {

    // Exchange metadata
    ret = mt_initial_setup();
    if (ret < 0)
      LP_THROW(-ret, "Communication error");

    // Get the CQs
    std::shared_ptr<tcm_cq> msg_cq   = nfr->ep_msg->get_cq().lock();
    std::shared_ptr<tcm_cq> frame_cq = nfr->ep_msg->get_cq().lock();
    fi_cq_data_entry        de;
    fi_cq_err_entry         err;

    // Implicitly paused as per the protocol
    t_state.ctrl = S_C_PAUSE_LOCAL_SYNC | S_F_PAUSE_LOCAL_SYNC;

    char   cbuf[LGMP_MSGS_SIZE];
    void * buf = (void *) cbuf;
    size_t size;

    timespec upd_dl = lp_get_deadline(t_state.net_timeout);

    do {
      int num_f = t_state.lgmp->num_clients(LGMP_Q_FRAME);
      int num_c = t_state.lgmp->num_clients(LGMP_Q_POINTER);

      // Pause when no LGMP clients

      if (num_c == 0 && !(t_state.ctrl & S_C_PAUSE_LOCAL_SYNC)) {
        t_state.ctrl |= S_C_PAUSE_LOCAL_UNSYNC;
        ret = mt_send_state(SESSION_CURSOR, NFR_STATE_PAUSE);
        switch (ret) {
          case 1:
          case 0:
            t_state.ctrl |= S_C_PAUSE_LOCAL_SYNC;
            t_state.ctrl &= ~S_C_PAUSE_LOCAL_UNSYNC;
            break;
          case -FI_EAGAIN: break;
          default:
            lp_log_error("Fabric error: %s", fi_strerror(-ret));
            return 0;
        }
      }

      if (num_f == 0 && !(t_state.ctrl & S_F_PAUSE_LOCAL_SYNC)) {
        t_state.ctrl |= S_F_PAUSE_LOCAL_UNSYNC;
        ret = mt_send_state(SESSION_FRAME, NFR_STATE_PAUSE);
        switch (ret) {
          case 1:
          case 0:
            t_state.ctrl |= S_F_PAUSE_LOCAL_SYNC;
            t_state.ctrl &= ~S_F_PAUSE_LOCAL_UNSYNC;
            break;
          case -FI_EAGAIN: break;
          default:
            lp_log_error("Fabric error: %s", fi_strerror(-ret));
            return 0;
        }
      }

      // Resume when LGMP clients connected

      if (num_c > 0 && (t_state.ctrl & S_C_PAUSE_LOCAL_SYNC)) {
        ret = mt_send_state(SESSION_CURSOR, NFR_STATE_RESUME);
        switch (ret) {
          case 1:
          case 0:
            t_state.ctrl &= ~(S_C_PAUSE_LOCAL_UNSYNC | S_C_PAUSE_LOCAL_SYNC);
            break;
          case -FI_EAGAIN: break;
          default:
            lp_log_error("Fabric error: %s", fi_strerror(-ret));
            return 0;
        }
      }

      if (num_f > 0 && (t_state.ctrl & S_F_PAUSE_LOCAL_SYNC)) {
        ret = mt_send_state(SESSION_FRAME, NFR_STATE_RESUME);
        switch (ret) {
          case 1:
          case 0:
            t_state.ctrl &= ~(S_F_PAUSE_LOCAL_UNSYNC | S_F_PAUSE_LOCAL_SYNC);
            break;
          case -FI_EAGAIN: break;
          default:
            lp_log_error("Fabric error: %s", fi_strerror(-ret));
            return 0;
        }
      }

      // Post receives

      ret = mt_post_recvs();
      if (ret < 0)
        LP_THROW(-ret, "Communication error");

      // Poll CQs

      ret = msg_cq->poll(&de, &err, 1, nullptr, 0);
      if (ret == 1) {
        lp_log_trace("Incoming message completion");
        ret = mt_process_comp(&de);
      } else if (ret != 0 && ret != -FI_EAGAIN)
        LP_THROW(-ret, "Message decode failure");

      ret = frame_cq->poll(&de, &err, 1, nullptr, 0);
      if (ret == 1) {
        lp_log_trace("Incoming frame completion");
        ret = mt_process_comp(&de);
      } else if (ret != 0 && ret != -FI_EAGAIN)
        LP_THROW(-ret, "Message decode failure");

      // Flush queues

      mt_flush_lgmp_pending_cursor_in();
      mt_flush_lgmp_pending_cursor_out();
      mt_flush_lgmp_pending_frame_in();

      // Reclaim buffers

      mt_get_unused_bufs();
      mt_sync_remote_buffers();

      // Check for incoming messages from LGMP clients

      size = LGMP_MSGS_SIZE;
      ret  = t_state.lgmp->process(buf, &size);
      switch (ret) {
        case 0: break;
        case 1: ret = mt_process_lgmp_in(buf, size); break;
        default:
          lp_log_error("Unhandled error: %s", strerror(-ret));
          LP_THROW(-ret, "Unhandled LGMP error");
      }
    } while (!exit_flag && !lp_check_deadline(upd_dl));

    if (exit_flag)
      throw lp_exit(LP_EXIT_LOCAL);
    LP_THROW(ETIMEDOUT, "Remote peer timeout");

  } catch (lp_exit & e) {
    lp_log_info("%s", e.what());
    return 0;
  } catch (tcm_exception & e) {
    std::string desc = e.full_desc();
    lp_log_error("%s", desc.c_str());
    return 0;
  }
  return 0;
}

int main(int argc, char ** argv) {
  if (getuid() == 0 || getuid() != geteuid()) {
    printf("===============================================================\n"
           "Do not run LGProxy as the root user or with setuid!            \n"
           "If you are running into permission issues, it is highly likely \n"
           "that you have missed a step in the instructions. LGProxy never \n"
           "requires root permissions; please check the instructions for   \n"
           "detailed troubleshooting steps.                                \n"
           "                                                               \n"
           "Quick troubleshooting steps:                                   \n"
           " - Is the locked memory limit (ulimit -l) high enough?         \n"
           " - Are the permissions of the shared memory file correct?      \n"
           " - Is the RDMA subsystem configured correctly?                 \n"
           "===============================================================\n");
    return 1;
  }

  lp_set_log_level();
  tcm__log_set_level(TCM__LOG_TRACE);
  signal(SIGINT, exit_handler);

  shared_ptr<lp_sink_opts> opts;
  shared_ptr<lp_shmem>     shm;

  int ret;

  try {
    opts = make_shared<lp_sink_opts>(argc, argv);
    lp_log_info("Initializing");
    shm = make_shared<lp_shmem>(opts->file, opts->shm_len);
    ret = tcm_init(nullptr);
    if (ret < 0)
      LP_THROW(-ret, "TCM init failed");
  } catch (tcm_exception & e) {
    if (e.return_code() == EAGAIN) {
      lp_print_usage(0);
    } else {
      std::string desc = e.full_desc();
      lp_log_error("%s", desc.c_str());
      lp_log_error("For help, run %s -h", argv[0]);
    }
    return 1;
  }

  try {
    NFRClientOpts c_opts;
    memset(&c_opts, 0, sizeof(c_opts));
    c_opts.api_version = opts->fabric_version;
    c_opts.build_ver   = LG_BUILD_VERSION;
    c_opts.src_addr    = opts->src_addr;
    c_opts.dst_addr    = opts->server_addr;
    c_opts.dst_port    = opts->server_port;
    c_opts.transport   = opts->transport;
    c_opts.timeout_ms  = opts->timeout;

    NFRClientResource res;
    res.ep_frame   = 0;
    res.ep_msg     = 0;
    res.fabric     = 0;
    res.peer_frame = FI_ADDR_UNSPEC;
    res.peer_msg   = FI_ADDR_UNSPEC;

    ret = NFRClientCreate(c_opts, res);
    if (ret < 0) {
      lp_log_error("Failed to connect to server: %s", fi_strerror(-ret));
      return 1;
    }

    // Set initial conditions

    t_state.mtd.clear();
    t_state.lgmp.reset();
    t_state.mbuf.reset();
    t_state.rshm.reset(new lp_rdma_shmem(res.fabric, shm));
    t_state.resrc = &res;

    t_state.lgmp_interval = opts->interval;
    t_state.lgmp_timeout  = 1000;
    t_state.net_interval  = opts->interval;
    t_state.net_timeout   = opts->timeout;
    t_state.page_size     = tcm_get_page_size();

    t_state.msg_cq   = res.ep_msg->get_cq().lock();
    t_state.frame_cq = res.ep_frame->get_cq().lock();
    t_state.opts     = opts;

    // Start main function

    main_thread(nullptr);

    // Cleanup

    t_state.lgmp.reset();
    t_state.rshm.reset();
    t_state.mbuf.reset();
    t_state.msg_cq.reset();
    t_state.frame_cq.reset();
    while (!t_state.pending_frame.empty()) {
      t_state.pending_frame.pop_front();
    }
    while (!t_state.pending_cursor.empty()) {
      t_state.pending_cursor.pop_front();
    }
    while (!t_state.lgmp_cur_tx.empty()) {
      t_state.lgmp_cur_tx.pop_front();
    }

    t_state.resrc = 0;
    res.ep_frame  = 0;
    res.ep_msg    = 0;
    res.fabric    = 0;

  } catch (tcm_exception & e) {
    std::string desc = e.full_desc();
    lp_log_error("%s", desc.c_str());
    return 1;
  }
  return 0;
}