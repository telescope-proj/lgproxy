#include "common.hpp"

extern volatile int  exit_flag;
extern t_state_frame f_state;
extern shared_state  state;

#define PEER (state.resrc->peer_frame)
#define EP (state.resrc->ep_frame)

// Incoming message processing

static int ft_process_in_msg(NFRClientFrameBuf * msg, uint64_t size) {
  if (size < sizeof(*msg))
    return -EBADMSG;
  if (!f_state.wait_fb) {
    lp_log_error("[F] Framebuffer message received at wrong state");
    return -EBADMSG;
  }
  if (f_state.remote_fb.offsets.size())
    f_state.remote_fb.clear();
  if (!f_state.remote_fb.import(msg, (uint32_t) size, PEER)) {
    lp_log_error("[F] Failed to import framebuffer data");
    return -EBADMSG;
  }
  f_state.wait_fb = false;
  return 0;
}

static int ft_process_in_msg(NFRClientAck * msg, uint64_t size) {
  if (size < sizeof(*msg))
    return -EBADMSG;
  if (msg->header.type != NFR_MSG_CLIENT_ACK)
    return -EBADMSG;
  if (msg->type != NFR_BUF_FRAME)
    return -EBADMSG;
  int n_idx = size - sizeof(*msg);
  if (!f_state.remote_fb.reclaim(msg->indexes, n_idx))
    return -EBADMSG;
  return 0;
}

static int ft_process_in_msg(NFRState * msg, uint64_t size) {
  if (size < sizeof(*msg))
    return -EBADMSG;
  if (msg->header.type != NFR_MSG_STATE)
    return -EBADMSG;
  switch (msg->state) {
    case NFR_STATE_KA:
      f_state.net_deadline = lp_get_deadline(state.net_timeout);
      return 0;
    case NFR_STATE_DISCONNECT:
      state.ctrl &= S_EXIT;
      throw lp_exit(LP_EXIT_REMOTE);
    case NFR_STATE_PAUSE: state.ctrl &= S_F_PAUSE_REMOTE; return 0;
    case NFR_STATE_RESUME: state.ctrl &= ~S_F_PAUSE_REMOTE; return 0;
    default: return -EBADMSG;
  }
}

static int ft_process_in_comp(fi_cq_data_entry * de) {
  lp_comp_info * info = f_state.mbuf->get_context(de->op_context);
  void *         buf  = f_state.mbuf->get_buffer(info);

  NFRHeader * hdr = static_cast<NFRHeader *>(buf);
  if (!nfrHeaderVerify(hdr, de->len)) {
    lp_log_debug("[F] Invalid header data");
    return -EBADMSG;
  }

  int ret;
  while (1) {
    uint32_t st = state.ctrl.load();
    if ((st & (S_C_INIT | S_F_INIT)) == 0) {
      switch (hdr->type) {
        case NFR_MSG_CLIENT_ACK:
          lp_log_trace("[F] Processing client ACK");
          ret = ft_process_in_msg(static_cast<NFRClientAck *>(buf), de->len);
          break;
        case NFR_MSG_CLIENT_FRAME_BUF:
          lp_log_trace("[F] Processing client frame buffer message");
          ret =
              ft_process_in_msg(static_cast<NFRClientFrameBuf *>(buf), de->len);
          break;
        case NFR_MSG_STATE:
          ret = ft_process_in_msg(static_cast<NFRState *>(buf), de->len);
          break;
        default:
          lp_log_trace("[F] Unexpected message type %d", hdr->type);
          ret = -EBADMSG;
      }
    } else if (st & S_F_PAUSE_REMOTE) {
      switch (hdr->type) {
        case NFR_MSG_CLIENT_ACK:
        case NFR_MSG_CURSOR_ALIGN:
          lp_log_debug("Resuming session (implicit)");
          state.ctrl &= ~S_F_PAUSE_REMOTE;
          continue;
        case NFR_MSG_STATE:
          ret = ft_process_in_msg(static_cast<NFRState *>(buf), de->len);
          break;
        default: ret = -EBADMSG;
      }
    }
    break;
  }

  f_state.mbuf->unlock(info);
  return ret;
}

// Outgoing message processing

static int ft_process_out_comp(fi_cq_data_entry * de) {
  lp_comp_info * info = f_state.mbuf->get_context(de->op_context);
  f_state.mbuf->unlock(info);
  return 0;
}

static int ft_send_frame_meta(pending_frame_meta & pend) {
  int idx = f_state.mbuf->lock(TAG_FRAME_TX);
  if (idx < 0)
    return -EAGAIN;

  void *         buf  = f_state.mbuf->get_buffer(TAG_FRAME_TX, idx);
  lp_comp_info * info = f_state.mbuf->get_context(TAG_FRAME_TX, idx);
  uint64_t       off  = f_state.mbuf->get_offset(TAG_FRAME_TX, idx);
  tcm_mem *      mem  = f_state.mbuf->get_mem();
  memcpy(buf, &pend.mtd, sizeof(pend.mtd));

  ssize_t ret = EP->send(*mem, PEER, info->fctx, off, sizeof(pend.mtd));
  if (ret == 0)
    pend.pending = true;
  return ret;
}

static int ft_process_write_comp(fi_cq_data_entry * de) {
  lp_comp_info * info = f_state.mbuf->get_context(de->op_context);

  // Release the frame
  frame_context * f = static_cast<frame_context *>(info->extra);
  if (!f->ack) {
    LGMP_STATUS s = f_state.lgmp->ack_msg();
    if (s != LGMP_OK)
      common_update_lgmp_state(THREAD_FRAME, s);
    // If s != LGMP_OK we probably timed out or the host died, so the frame can
    // be considered acknowledged in any case
    f->ack = true;
  }

  if ((state.ctrl & (S_F_PAUSE_LOCAL_SYNC | S_F_PAUSE_LOCAL_UNSYNC |
                     S_F_PAUSE_REMOTE)) == 0) {
    pending_frame_meta tmp;
    f_state.unproc_frame.push(tmp);
    pending_frame_meta & pend = f_state.unproc_frame.back();
    pend.pending              = false;
    pend.mtd.header           = NFRHeaderCreate(NFR_MSG_FRAME_METADATA);
    pend.mtd.buffer           = f->index;
    pend.mtd.width            = f->fi.dataWidth;
    pend.mtd.height           = f->fi.dataHeight;
    pend.mtd.row_bytes        = f->fi.pitch;
    pend.mtd.frame_type       = (NFRFrameType) f->fi.type;
    pend.mtd.frame_rotation   = (NFRFrameRotation) f->fi.rotation;
    pend.mtd.flags            = f->fi.flags;
  }

  f_state.mbuf->unlock(info);
  return 0;
}

static void ft_print_fabric_error(fi_cq_err_entry & err) {
  char                    ebuf[256];
  lp_comp_info *          comp = f_state.mbuf->get_context(err.op_context);
  std::shared_ptr<tcm_cq> cq   = EP->get_cq().lock();
  fi_cq_strerror(cq->raw(), err.prov_errno, err.err_data, ebuf, 256);
  lp_log_error("Fabric Error; tag: %s (%d), idx: %d, error: %s (%d), "
               "prov_err: %s",
               get_buffer_tag_str(comp->_buftag), comp->_buftag, comp->_index,
               fi_strerror(err.err), err.err, ebuf);
  cq.reset();
}

// Completion router

static int ft_process_comp(fi_cq_data_entry * de) {
  ssize_t        ret;
  lp_comp_info * info = f_state.mbuf->get_context(de->op_context);
  uint8_t        tag  = f_state.mbuf->get_tag(info);
  switch (tag) {
    case TAG_FRAME_RX: ret = ft_process_in_comp(de); break;
    case TAG_FRAME_TX: ret = ft_process_out_comp(de); break;
    case TAG_FRAME_WRITE: ret = ft_process_write_comp(de); break;
    default:
      lp_log_error("Invalid completion tag: %d", tag);
      throw tcm_exception(EINVAL, __FILE__, __LINE__, "Invalid completion tag");
  }
  if (ret == 0)
    f_state.mbuf->unlock(info);
  return ret;
}

// LGMP incoming message processing

static int ft_process_in_lgmp(lp_lgmp_msg & msg) {
  // Unlike the cursor buffer, it wastes a significant amount of memory and time
  // to copy the frame to an intermediate buffer. The update speed and users'
  // latency sensitivity for frames is also significantly lower than that of
  // cursor data.
  KVMFRFrame * f = msg.frame();
  ssize_t      ret;

  int ridx = f_state.remote_fb.lock();
  if (ridx < 0) {
    return -ENOBUFS;
  }

  int lidx = f_state.mbuf->lock(TAG_FRAME_WRITE);
  if (lidx < 0) {
    f_state.remote_fb.unlock(ridx);
    return -EAGAIN;
  }

  void *         tex  = (void *) (((uint8_t *) f) + f->offset + FB_WP_SIZE);
  tcm_remote_mem rbuf = f_state.remote_fb.export_rmem();

  lp_comp_info * info = f_state.mbuf->get_context(TAG_FRAME_WRITE, lidx);
  if (tex < state.rshm->mem->get_ptr()) {
    f_state.remote_fb.unlock(ridx);
    f_state.mbuf->unlock(TAG_FRAME_WRITE, lidx);
    assert(false && "Invalid frame buffer!");
    throw tcm_exception(EINVAL, __FILE__, __LINE__,
                        "Frame buffer out of range");
  }

  uint64_t off = (uintptr_t) tex - (uintptr_t) state.rshm->mem->get_ptr();

  lp_log_trace("Writing frame; L %p + %lu --> R %p + %lu / Key: %lu; Size: %lu",
               state.rshm->mem->get_ptr(), off, rbuf.addr,
               f_state.remote_fb.offsets[ridx], rbuf.u.rkey,
               f->pitch * f->dataHeight);
  lp_log_trace("Frame actual size: %lu", f->pitch * f->dataHeight);
  ret = EP->rwrite(*state.rshm->mem, rbuf, info->fctx, off,
                   f_state.remote_fb.offsets[ridx], f->pitch * f->dataHeight);
  switch (ret) {
    case 0: break;
    default:
      lp_log_error("Failed to send write: %s", fi_strerror(-ret));
      return ret; // otherwise triggers -Wimplicit-fallthrough
    case -FI_EAGAIN: return ret;
  }

  frame_context * fctx = static_cast<frame_context *>(info->extra);
  fctx->ack            = false;
  fctx->index          = ridx;
  memcpy(&fctx->fi, f, sizeof(fctx->fi));
  return 0;
}

// Main event loop

void * frame_thread(void * arg) {
  (void) arg;
  ssize_t ret;

  state.ctrl &= S_F_INIT;
  lp_log_info("[F] Initializing");

  try {
    lp_lgmp_msg msg;
    msg.size = 0;

    common_post_recv(THREAD_FRAME);

    // Wait for metadata thread to be ready
    pthread_mutex_lock(state.start_lock);
    if (exit_flag) {
      lp_log_info("[F] Exit requested");
      return 0;
    }

    state.ctrl |= S_F_PAUSE_REMOTE;
    state.ctrl &= ~S_F_INIT;
    lp_log_info("[F] Started");

    f_state.wait_fb       = true;
    f_state.net_deadline  = lp_get_deadline(state.net_timeout);
    f_state.lgmp_deadline = lp_get_deadline(state.lgmp_timeout);

    while (1) {
      common_keep_lgmp_connected(THREAD_FRAME);
      common_post_recv(THREAD_FRAME);

      // Check for new network messages
      fi_cq_data_entry de;
      fi_cq_err_entry  err;
      ret = common_poll_cq(THREAD_FRAME, &de, &err);
      switch (ret) {
        case 1:
          f_state.unproc_q.push(de);
          f_state.net_deadline = lp_get_deadline(state.net_timeout);
        case -FI_EAGAIN:
        case 0: break;
        case -FI_EAVAIL: {
          ft_print_fabric_error(err);
          LP_THROW(err.err, "Fabric error, cannot continue");
        }
        default:
          lp_log_error("Fabric poll failed: %s", fi_strerror(-ret));
          break;
      }

      // Process any unprocessed completions
      while (!f_state.unproc_q.empty()) {
        fi_cq_data_entry & de   = f_state.unproc_q.front();
        lp_comp_info *     comp = f_state.mbuf->get_context(de.op_context);
        lp_log_trace("Processing queue item; buftag: %s (%d), index: %d",
                     get_buffer_tag_str(comp->_buftag), comp->_buftag,
                     comp->_index);
        ret = ft_process_comp(&de);
        switch (ret) {
          case 0: f_state.unproc_q.pop_front(); break;
          case -EBADMSG: lp_log_error("Invalid message received"); return 0;
          default: lp_log_error("Error"); return 0;
        }
      }

      uint32_t st = state.ctrl.load();

      // When the remote side requested a pause, remove all pending requests
      if (st & S_F_PAUSE_REMOTE) {
        f_state.unproc_frame.clear();
        f_state.remote_fb.unlock_all();
      }

      // Check for LGMP messages, only if there are available remote buffers
      bool active = st & (S_F_PAUSE_LOCAL_SYNC | S_F_PAUSE_LOCAL_UNSYNC |
                          S_F_PAUSE_REMOTE);

      if (active && f_state.remote_fb.avail()) {
        if (msg.size == 0) {
          msg = common_get_lgmp_message(THREAD_FRAME);
          if (msg.size < 0) {
            common_update_lgmp_state(THREAD_FRAME, (LGMP_STATUS) -msg.size);
            msg.size = 0;
          }
        }

        if (msg.size > 0) {
          ret = ft_process_in_lgmp(msg);
          switch (ret) {
            case -ENOBUFS:
              f_state.lgmp->ack_msg();
              msg.size = 0;
              break; // cause -Wimplicit-fallthrough
            case 0: msg.size = 0;
            default: break;
          }
        }
      }

      // Check for completed frames

      while (active && !f_state.unproc_frame.empty()) {
        pending_frame_meta & pend = f_state.unproc_frame.front();
        ret                       = ft_send_frame_meta(pend);
        if (ret < 0 && ret != -FI_EAGAIN)
          return 0;
        f_state.unproc_frame.pop_front();
      }

      // Exit conditions

      if (st & S_EXIT) {
        lp_log_error("[F] Exiting");
        return 0;
      }

      if (exit_flag) {
        lp_log_error("[F] User requested exit");
        throw lp_exit(LP_EXIT_LOCAL);
      }

      if (lp_check_deadline(f_state.net_deadline)) {
        LP_THROW(ETIMEDOUT, "Remote peer timeout");
      }
    };

  } catch (tcm_exception & e) {
    std::string desc = e.full_desc();
    lp_log_error("%s", desc.c_str());
    state.ctrl |= S_EXIT;
    return 0;
  } catch (lp_exit & e) {
    state.ctrl |= S_EXIT;
    return 0;
  }
  return 0;
}
