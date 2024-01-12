#include "common.hpp"
#include "lg_build_version.h"
#include "lp_build_version.h"

extern volatile int   exit_flag;
extern t_state_cursor c_state;
extern shared_state   state;

#define PEER (state.resrc->peer_msg)
#define EP (state.resrc->ep_msg)

static int ct_gather_host_metadata(NFRHostMetadataConstructor & c) {
  lp_lgmp_client_q q(state.rshm->shm, state.lgmp_timeout, state.lgmp_interval);
  q.bind_exit_flag(&exit_flag);
  LGMP_STATUS s = q.init();
  if (exit_flag)
    return -ECANCELED;
  if (s == LGMP_ERR_INVALID_VERSION) {
    lp_log_trace("Invalid version");
    return -ENOTSUP;
  }
  if (s != LGMP_OK) {
    lp_log_trace("LGMP error: %s", lgmpStatusString(s));
    return -EAGAIN;
  }
  lp_kvmfr_udata_to_nfr(q.udata, q.udata_size, c);
  return 0;
}

// Cursor thread: Incoming message processing

static int ct_process_in_msg(NFRClientAck * msg, uint64_t size) {
  if (size < sizeof(*msg))
    return -EBADMSG;
  if (msg->type != NFR_BUF_CURSOR_DATA)
    return -EBADMSG;
  int n_idx = size - sizeof(*msg);
  if (!c_state.remote_cb.reclaim(msg->indexes, n_idx))
    return -EBADMSG;
  return 0;
}

static int ct_process_in_msg(NFRState * msg, uint64_t size) {
  if (size < sizeof(*msg))
    return -EBADMSG;
  switch (msg->state) {
    case NFR_STATE_DISCONNECT: throw lp_exit(LP_EXIT_REMOTE);
    case NFR_STATE_KA: return 0;
    case NFR_STATE_PAUSE: state.ctrl &= S_C_PAUSE_REMOTE; return 0;
    case NFR_STATE_RESUME: state.ctrl &= ~S_C_PAUSE_REMOTE; return 0;
    default: return -EBADMSG;
  }
}

static int ct_process_in_msg(NFRCursorAlign * msg, uint64_t size) {
  if (size < sizeof(*msg))
    return -EBADMSG;
  KVMFRSetCursorPos pos;
  pos.msg.type = KVMFR_MESSAGE_SETCURSORPOS;
  pos.x        = msg->x;
  pos.y        = msg->y;
  c_state.unproc_align.push(pos);
  return 0;
}

static int ct_process_in_msg(NFRClientCursorBuf * msg, uint64_t size) {
  if (size < sizeof(*msg))
    return -EBADMSG;
  if (c_state.remote_cb.offsets.size())
    c_state.remote_cb.clear();
  if (!c_state.remote_cb.import(msg, size, PEER))
    return -EBADMSG;
  return 0;
}

static int ct_process_in_comp(fi_cq_data_entry * de) {
  lp_log_trace("[C] Processing completion");
  lp_comp_info * info = c_state.mbuf->get_context(de->op_context);
  void *         buf  = c_state.mbuf->get_buffer(info);

  NFRHeader * hdr = static_cast<NFRHeader *>(buf);
  if (!nfrHeaderVerify(hdr, de->len))
    return -EBADMSG;

  int ret;
  while (1) {
    uint32_t st = state.ctrl.load();
    if (st & (S_C_INIT | S_F_INIT) == 0) {
      switch (hdr->type) {
        case NFR_MSG_CLIENT_ACK:
          ret = ct_process_in_msg(static_cast<NFRClientAck *>(buf), de->len);
          break;
        case NFR_MSG_STATE:
          ret = ct_process_in_msg(static_cast<NFRState *>(buf), de->len);
          break;
        case NFR_MSG_CURSOR_ALIGN:
          ret = ct_process_in_msg(static_cast<NFRCursorAlign *>(buf), de->len);
          break;
        default: ret = -EBADMSG; break;
      }
    } else if (st & S_C_INIT) {
      switch (hdr->type) {
        case NFR_MSG_STATE:
          ret = ct_process_in_msg(static_cast<NFRState *>(buf), de->len);
          break;
        case NFR_MSG_CURSOR_ALIGN:
          ret = ct_process_in_msg(static_cast<NFRCursorAlign *>(buf), de->len);
          break;
        case NFR_MSG_CLIENT_CURSOR_BUF:
          ret = ct_process_in_msg(static_cast<NFRClientCursorBuf *>(buf),
                                  de->len);
          break;
        default: ret = -EBADMSG; break;
      }
    } else if (st & S_C_PAUSE_REMOTE) {
      switch (hdr->type) {
        case NFR_MSG_CLIENT_ACK:
        case NFR_MSG_CURSOR_ALIGN:
          lp_log_debug("Resuming session (implicit)");
          state.ctrl &= ~S_C_PAUSE_REMOTE;
          continue;
        case NFR_MSG_STATE:
          ret = ct_process_in_msg(static_cast<NFRState *>(buf), de->len);
          break;
        default: ret = -EBADMSG; break;
      }
    } else if (st & (S_C_PAUSE_LOCAL_SYNC | S_C_PAUSE_LOCAL_UNSYNC)) {
      switch (hdr->type) {
        case NFR_MSG_CLIENT_ACK:
          ret = ct_process_in_msg(static_cast<NFRClientAck *>(buf), de->len);
          break;
        case NFR_MSG_STATE:
          ret = ct_process_in_msg(static_cast<NFRState *>(buf), de->len);
          break;
        case NFR_MSG_CURSOR_ALIGN: ret = 0; break;
        default: ret = -EBADMSG; break;
      }
    }
    break;
  }

  c_state.mbuf->unlock(info);
  return ret;
}

// Cursor thread: Outgoing message processing

static int ct_process_out_comp(fi_cq_data_entry * de) {
  lp_comp_info * info = c_state.mbuf->get_context(de->op_context);
  c_state.mbuf->unlock(info);
  return 0;
}

static int ct_process_write_comp(fi_cq_data_entry * de) {
  lp_comp_info *   info = c_state.mbuf->get_context(de->op_context);
  cursor_context * c    = static_cast<cursor_context *>(info->extra);

  if (c->lidx >= 0)
    c_state.mbuf->unlock(TAG_CURSOR_SHAPE, c->lidx);

  if (c->ridx < 0 || c->ridx >= (int16_t) c_state.remote_cb.offsets.size())
    throw tcm_exception(EINVAL, __FILE__, __LINE__, "Bad completion data");

  {
    NFRCursorMetadata mtd;
    c_state.unproc_cursor.push(mtd);
  }

  NFRCursorMetadata & mtd = c_state.unproc_cursor.back();
  mtd.header              = NFRHeaderCreate(NFR_MSG_CURSOR_METADATA);
  mtd.buffer              = c->ridx;
  mtd.flags               = c->flags;
  mtd.x                   = c->x;
  mtd.y                   = c->y;
  mtd.hx                  = c->hx;
  mtd.hy                  = c->hy;
  mtd.row_bytes           = c->rb;
  mtd.width               = c->w;
  mtd.height              = c->h;

  return 0;
}

// Cursor thread: Completion router

static int ct_process_comp(fi_cq_data_entry * de) {
  ssize_t        ret;
  lp_comp_info * info = c_state.mbuf->get_context(de->op_context);
  uint8_t        tag  = c_state.mbuf->get_tag(info);
  switch (tag) {
    case TAG_MSG_RX: ret = ct_process_in_comp(de); break;
    case TAG_MSG_TX: ret = ct_process_out_comp(de); break;
    case TAG_CURSOR_SHAPE: ret = ct_process_write_comp(de); break;
    default: ret = -EINVAL;
  }
  if (ret == 0)
    c_state.mbuf->unlock(info);
  return ret;
}

// Cursor thread: LGMP incoming message processing

static int ct_send_local_pause() {
  int idx = c_state.mbuf->lock(TAG_MSG_TX);
  if (idx < 0)
    return -EAGAIN;

  lp_comp_info * info = c_state.mbuf->get_context(TAG_MSG_TX, idx);
  void *         buf  = c_state.mbuf->get_buffer(info);
  uint64_t       off  = c_state.mbuf->get_offset(info);
  tcm_mem *      mem  = c_state.mbuf->get_mem();
  NFRState *     st   = static_cast<NFRState *>(buf);
  st->header          = NFRHeaderCreate(NFR_MSG_STATE);
  st->state           = NFR_STATE_PAUSE;
  return EP->send(*mem, PEER, info->fctx, off, sizeof(*st));
}

static int ct_send_local_resume() {
  int idx = c_state.mbuf->lock(TAG_MSG_TX);
  if (idx < 0)
    return -EAGAIN;

  lp_comp_info * info = c_state.mbuf->get_context(TAG_MSG_TX, idx);
  void *         buf  = c_state.mbuf->get_buffer(info);
  uint64_t       off  = c_state.mbuf->get_offset(info);
  tcm_mem *      mem  = c_state.mbuf->get_mem();
  NFRState *     st   = static_cast<NFRState *>(buf);
  st->header          = NFRHeaderCreate(NFR_MSG_STATE);
  st->state           = NFR_STATE_RESUME;
  return EP->send(*mem, PEER, info->fctx, off, sizeof(*st));
}

static int ct_send_cursor(cursor_context & ctx) {
  ssize_t    ret;
  buffer_tag tag = ctx.ridx >= 0 ? TAG_CURSOR_SHAPE : TAG_MSG_TX;
  tcm_mem *  mem = c_state.mbuf->get_mem();

  switch (tag) {
    case TAG_CURSOR_SHAPE: {
      tcm_remote_mem rmem = c_state.remote_cb.export_rmem();
      uint64_t       off  = c_state.mbuf->get_offset(tag, ctx.lidx);
      lp_comp_info * info = c_state.mbuf->get_context(tag, ctx.lidx);
      return EP->rwrite(*mem, rmem, info->fctx, off,
                        c_state.remote_cb[ctx.ridx], ctx.rb * ctx.h);
    }
    case TAG_MSG_TX: {
      int idx = c_state.mbuf->lock(TAG_MSG_TX);
      if (idx < 0)
        return -EAGAIN;
      void *              buf  = c_state.mbuf->get_buffer(tag, idx);
      uint64_t            off  = c_state.mbuf->get_offset(tag, idx);
      lp_comp_info *      info = c_state.mbuf->get_context(tag, idx);
      NFRCursorMetadata * mtd  = static_cast<NFRCursorMetadata *>(buf);
      mtd->header              = NFRHeaderCreate(NFR_MSG_CURSOR_METADATA);
      mtd->buffer              = -1;
      mtd->x                   = ctx.x;
      mtd->y                   = ctx.y;
      mtd->hx                  = ctx.hx;
      mtd->hy                  = ctx.hy;
      mtd->flags               = ctx.flags;
      mtd->format              = 0;
      mtd->width               = 0;
      mtd->height              = 0;
      mtd->row_bytes           = 0;
      ret = EP->send(*mem, PEER, info->fctx, off, sizeof(*mtd));
      if (ret < 0)
        c_state.mbuf->unlock(info);
      return ret;
    }
    default:
      assert(false && "Unknown buffer tag");
      throw tcm_exception(EINVAL, __FILE__, __LINE__, "Unknown buffer tag");
  }
}

static int ct_process_in_lgmp(lp_lgmp_msg * msg) {
  // We do not initiate the write from here, as network operations are
  // (relatively) slow. The cursor info is pushed into a local queue to clear
  // out the LGMP queue as quickly as possible, and another function processes
  // anything in the cursor queue.
  cursor_context     upd;
  KVMFRCursor *      c = msg->cursor();
  lp_lgmp_client_q * q = c_state.lgmp.get();
  upd.x                = c->x;
  upd.y                = c->y;
  upd.hx               = c->hx;
  upd.hy               = c->hy;
  upd.rb               = c->pitch;
  upd.w                = c->width;
  upd.h                = c->height;
  upd.flags            = msg->udata;
  upd.lidx             = -1;
  upd.ridx             = -1;
  if (msg->udata & CURSOR_FLAG_SHAPE) {
    // For now, we drop the cursor shape when there are no free buffers
    // Reserve local buffer
    upd.lidx = c_state.mbuf->lock(TAG_CURSOR_SHAPE);
    if (upd.lidx >= 0) {
      // Reserve remote buffer
      upd.ridx = c_state.remote_cb.lock();
      if (upd.ridx >= 0) {
        // Copy cursor texture into local buffer
        void * buf = c_state.mbuf->get_buffer(TAG_CURSOR_SHAPE, upd.lidx);
        void * tex = (void *) (c + 1);
        memcpy(buf, tex, c->pitch * c->height);
      } else {
        lp_log_warn("No available remote cursor buffers!");
        c_state.mbuf->unlock(TAG_CURSOR_SHAPE, upd.lidx);
      }
    } else {
      lp_log_warn("No available cursor buffers!");
    }
  }

  // The metadata is pushed into the queue regardless of whether the shape was
  // dropped, since the amount of data is small enough to queue without much
  // concern
  c_state.cursor_q.push(upd);
  LGMP_STATUS s = q->ack_msg();
  if (s != LGMP_OK) {
    lp_log_warn("LGMP queue ack failed: %s", lgmpStatusString(s));
    common_update_lgmp_state(THREAD_CURSOR, s);
  }

  return 0;
}

// Cursor main thread

void * cursor_thread(void * arg) {
  (void) arg;
  ssize_t ret;

  try {

    // Collect metadata from the LGMP host. We start an LGMP session, collect
    // the metadata, and immediately close it, and send the metadata over the
    // network to the peer.

    char name[256];
    snprintf(name, 256, "LGProxy %d.%d.%d-%s; LG %s; LF %d.%d",
             LP_VERSION_MAJOR, LP_VERSION_MINOR, LP_VERSION_PATCH,
             LP_BUILD_VERSION, LG_BUILD_VERSION, FI_MAJOR(fi_version()),
             FI_MINOR(fi_version()));
    uint16_t name_len = strnlen(name, 256);

    tcm_mem * mem      = c_state.mbuf->get_mem();
    void *    buf      = c_state.mbuf->get_buffer(TAG_MSG_TX, 0);
    size_t    full_len = c_state.mbuf->get_slot_size(TAG_MSG_TX) *
                      c_state.mbuf->get_slot_count(TAG_MSG_TX);
    uint8_t                    proxied = 1;
    NFRHostMetadataConstructor c(buf, full_len);
    c.addField(NFR_F_NAME, name, name_len);
    c.addField(NFR_F_EXT_PROXIED, &proxied);
    lp_log_info("[C] Waiting for LGMP connection...");

    state.ctrl  = S_C_INIT;
    timespec dl = lp_get_deadline(state.lgmp_timeout);

    do {
      ret = ct_gather_host_metadata(c);
      switch (ret) {
        case -ECANCELED: lp_log_info("[C] Exit signal received"); return 0;
        case -ENOTSUP: lp_log_fatal("[C] LGMP version mismatch"); return 0;
        case -EAGAIN: continue;
        case 0: break;
        default: assert(false && "Unexpected state!");
      }
      if (ret == 0)
        break;
    } while (!exit_flag && !lp_check_deadline(dl));

    if (exit_flag || lp_check_deadline(dl)) {
      lp_log_info("[C] Exiting");
      state.ctrl |= S_EXIT;
      pthread_mutex_unlock(state.start_lock);
      return 0;
    }

    lp_log_info("[C] LGMP session established, relaying metadata");
    pthread_mutex_unlock(state.start_lock);
    dl = lp_get_deadline(state.net_timeout);

    while (1) {
      lp_comp_info * info = c_state.mbuf->get_context(TAG_MSG_TX, 0);
      uint64_t       off  = c_state.mbuf->get_offset(TAG_MSG_TX, 0);

      ret = EP->send(*mem, PEER, info->fctx, off, c.getUsed());
      if (ret >= 0)
        break;
      if (ret != -FI_EAGAIN)
        throw tcm_exception(-ret, __FILE__, __LINE__, "Fabric send failed");

      if ((state.ctrl & S_EXIT) || exit_flag || lp_check_deadline(dl)) {
        lp_log_info("[C] Exiting");
        state.ctrl |= S_EXIT;
        return 0;
      }
    }

    lp_log_info("Sent metadata, waiting for response");

    // Before we can relay cursor data, the client must send cursor texture
    // buffers to the server. Unlike frame data these cursor texture buffers
    // are permanently allocated for the duration of the session and cannot be
    // changed.

    dl        = lp_get_deadline(state.net_timeout);
    bool flag = false;

    do {
      if (flag)
        break;

      if (state.ctrl & S_EXIT)
        return 0;

      fi_cq_data_entry de;
      fi_cq_err_entry  err;
      common_post_recv(THREAD_CURSOR);
      ret = common_poll_cq(THREAD_CURSOR, &de, &err);
      if (ret == 1) {
        lp_comp_info * comp = c_state.mbuf->get_context(de.op_context);
        void *         buf  = c_state.mbuf->get_buffer(comp);
        lp_log_trace("Processing CQE; buftag: %s (%d), index: %d, buf: %p",
                     get_buffer_tag_str(comp->_buftag), comp->_buftag,
                     comp->_index, buf);
        ret = ct_process_comp(&de);
        if (ret < 0) {
          lp_log_error("[C] Message error: %s", fi_strerror(-ret));
          return 0;
        }
        if (comp->_buftag == TAG_MSG_RX) {
          if (!nfrHeaderVerify(buf, de.len)) {
            lp_log_error("[C] Invalid message header");
            return 0;
          }
          NFRHeader * hdr = static_cast<NFRHeader *>(buf);
          if (hdr->type == NFR_MSG_CLIENT_CURSOR_BUF)
            break;
        }
      }
    } while (!exit_flag && !lp_check_deadline(dl));

    if (exit_flag || lp_check_deadline(dl)) {
      lp_log_info("[C] Exiting");
      state.ctrl |= S_EXIT;
      return 0;
    }

    lp_log_info("[C] Initial metadata exchange complete");

    // The client must explicitly specify when to actually start the relay,
    // since there may still not be any LGMP clients connected on that side
    state.ctrl &= S_C_PAUSE_REMOTE;

  } catch (tcm_exception & e) {
    std::string desc = e.full_desc();
    lp_log_error("%s", desc.c_str());
    state.ctrl |= S_EXIT;
    return 0;
  }

  state.ctrl &= ~S_C_INIT;
  c_state.net_deadline = lp_get_deadline(state.net_timeout);

  // Main event loop

  try {
    lp_lgmp_msg msg;
    while (1) {

      if (exit_flag)
        throw lp_exit(LP_EXIT_LOCAL);

      if (lp_check_deadline(c_state.net_deadline))
        LP_THROW(ETIMEDOUT, "Remote peer timeout");

      uint8_t st = state.ctrl.load();
      if (st & S_EXIT) {
        lp_log_error("[C] Exiting");
        break;
      }

      // Keep resources active
      common_keep_lgmp_connected(THREAD_CURSOR);
      common_post_recv(THREAD_CURSOR);

      // Adjust local system pause state
      if ((st & S_C_PAUSE_LOCAL_SYNC) == 0 && !c_state.lgmp->connected()) {
        state.ctrl |= S_C_PAUSE_LOCAL_UNSYNC;
        state.ctrl &= ~S_C_PAUSE_LOCAL_SYNC;
        ret = ct_send_local_pause();
        switch (ret) {
          case 0:
            state.ctrl |= S_C_PAUSE_LOCAL_SYNC;
            state.ctrl &= ~S_C_PAUSE_LOCAL_UNSYNC;
            continue;
          case -FI_EAGAIN: break;
          default:
            lp_log_error("[C] Communication error: %s", fi_strerror(-ret));
            return 0;
        }
      } else {
        if (st & S_C_PAUSE_LOCAL_SYNC) {
          ret = ct_send_local_resume();
          switch (ret) {
            case 0:
              state.ctrl &= ~(S_C_PAUSE_LOCAL_SYNC | S_C_PAUSE_LOCAL_UNSYNC);
              continue;
            case -FI_EAGAIN: break;
            default:
              lp_log_error("[C] Communication error: %s", fi_strerror(-ret));
              return 0;
          }
        }
      }

      // Poll for a completion (both TX and RX)
      fi_cq_data_entry de;
      fi_cq_err_entry  err;
      ret = common_poll_cq(THREAD_CURSOR, &de, &err);
      if (ret == 1) {
        ret = ct_process_comp(&de);
        if (ret < 0) {
          lp_log_error("[C] Message error: %s", fi_strerror(-ret));
          return 0;
        }
        lp_comp_info * info = c_state.mbuf->get_context(de.op_context);
        c_state.mbuf->unlock(info);
      }

      // Only do LGMP relay while the system is active
      bool paused = (st & (S_C_PAUSE_LOCAL_SYNC | S_C_PAUSE_LOCAL_UNSYNC)) == 0;
      if (!paused) {
        // Get an LGMP message
        msg = common_get_lgmp_message(THREAD_CURSOR);
        if (msg.size < 0)
          continue;
        // Process LGMP messages
        ret = ct_process_in_lgmp(&msg);
        if (ret < 0 && ret != -FI_EAGAIN)
          return 0;
      }

      // Send cursor data
      while (!paused && !exit_flag && !c_state.cursor_q.empty()) {
        cursor_context & ctx = c_state.cursor_q.front();
        ret                  = ct_send_cursor(ctx);
        if (ret == 0) {
          c_state.cursor_q.pop_front();
        } else {
          lp_log_trace("[C] Failed to send message: %s", fi_strerror(-ret));
          break;
        }
      }
    }

  } catch (tcm_exception & e) {
    std::string desc = e.full_desc();
    lp_log_error("[C] %s", desc.c_str());
    state.ctrl |= S_EXIT;
    return 0;
  } catch (lp_exit & e) {
    lp_log_info("[C] User requested exit");
    state.ctrl |= S_EXIT;
    return 0;
  } catch (std::exception & e) {
    lp_log_error("[C] %s", e.what());
    state.ctrl |= S_EXIT;
    return 0;
  }
  return 0;
}
