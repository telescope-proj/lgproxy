// SPDX-License-Identifier: GPL-2.0-or-later
// Telescope Project
// Looking Glass Proxy (LGProxy)
// Copyright (c) 2022 - 2024, Telescope Project Developers

#include "common.hpp"

extern volatile int   exit_flag;
extern t_state_frame  f_state;
extern t_state_cursor c_state;
extern shared_state   state;

void common_keep_lgmp_connected(thread_id thr_id) {
  lp_lgmp_client_q * q    = 0;
  uint32_t           q_id = (uint32_t) -1;
  bool *             sub  = 0;
  switch (thr_id) {
    case THREAD_CURSOR:
      q_id = LGMP_Q_POINTER;
      q    = c_state.lgmp.get();
      sub  = &c_state.subbed;
      if (!q) {
        lp_log_trace("Resetting connection");
        c_state.lgmp.reset(new lp_lgmp_client_q(
            state.rshm->shm, state.lgmp_timeout, state.lgmp_interval));
        q = c_state.lgmp.get();
      }
      break;
    case THREAD_FRAME:
      q_id = LGMP_Q_FRAME;
      q    = f_state.lgmp.get();
      sub  = &f_state.subbed;
      if (!q) {
        lp_log_trace("Resetting connection");
        f_state.lgmp.reset(new lp_lgmp_client_q(
            state.rshm->shm, state.lgmp_timeout, state.lgmp_interval));
        q = f_state.lgmp.get();
      }
      break;
    default:
      assert(false && "Invalid thread number!");
      throw tcm_exception(EINVAL, _LP_FNAME_, __LINE__, "Bad thread number");
  }

  LGMP_STATUS s;
  if (!q->connected()) {
    *sub = false;
    q->bind_exit_flag(&exit_flag);
    s = q->connect(q_id);
    if (exit_flag)
      throw lp_exit(LP_EXIT_LOCAL);
    switch (s) {
      case LGMP_ERR_INVALID_VERSION:
        throw tcm_exception(ENOTSUP, _LP_FNAME_, __LINE__,
                            "Mismatched LGMP version!");
      case LGMP_OK:
      default: return;
    }
  }
  if (!*sub) {
    s = q->subscribe(q_id);
    if (s == LGMP_OK) {
      lp_log_debug("[%c] Resubscribed to host",
                   thr_id == THREAD_CURSOR ? 'C' : 'F');
      *sub = true;
    }
  }
}

void common_update_lgmp_state(thread_id thr_id, LGMP_STATUS retcode) {
  bool * sub = 0;
  switch (thr_id) {
    case THREAD_CURSOR: sub = &c_state.subbed; break;
    case THREAD_FRAME: sub = &f_state.subbed; break;
    default:
      assert(false && "Invalid thread number!");
      throw tcm_exception(EINVAL, _LP_FNAME_, __LINE__, "Bad thread number");
  }
  switch (retcode) {
    case LGMP_ERR_QUEUE_EMPTY:
    case LGMP_ERR_QUEUE_FULL:
    case LGMP_OK: return;
    case LGMP_ERR_INVALID_VERSION:
      throw tcm_exception(ENOTSUP, _LP_FNAME_, __LINE__,
                          "Mismatched LGMP version!");
    default: *sub = false; return;
  }
}

lp_lgmp_msg common_get_lgmp_message(thread_id thr_id) {
  lp_lgmp_msg        msg;
  lp_lgmp_client_q * q = 0;
  switch (thr_id) {
    case THREAD_CURSOR: q = c_state.lgmp.get(); break;
    case THREAD_FRAME: q = f_state.lgmp.get(); break;
    default:
      assert(false && "Invalid thread number!");
      throw tcm_exception(EINVAL, _LP_FNAME_, __LINE__, "Bad thread number");
  }
  msg = q->get_msg();
  if (msg.size < 0) {
    common_update_lgmp_state(thr_id, (LGMP_STATUS) -msg.size);
  }
  return msg;
}

void common_post_recv(thread_id thr_id) {
  ssize_t             ret;
  NFRServerResource * nfr  = state.resrc;
  tcm_endpoint *      ep   = 0;
  lp_mbuf *           mbuf = 0;
  buffer_tag          tag;
  fi_addr_t           peer = FI_ADDR_UNSPEC;
  assert(nfr);
  switch (thr_id) {
    case THREAD_FRAME:
      mbuf = f_state.mbuf.get();
      tag  = TAG_FRAME_RX;
      peer = nfr->peer_frame;
      ep   = nfr->ep_frame.get();
      break;
    case THREAD_CURSOR:
      mbuf = c_state.mbuf.get();
      tag  = TAG_MSG_RX;
      peer = nfr->peer_msg;
      ep   = nfr->ep_msg.get();
      break;
    default:
      lp_log_error("Invalid thread ID!");
      assert(false && "Invalid thread ID");
      throw tcm_exception(EINVAL, _LP_FNAME_, __LINE__, "Invalid thread ID");
  }

  assert(mbuf);
  assert(ep);
  int idx = -1;
  while ((idx = mbuf->lock(tag)) >= 0) {
    lp_log_trace("[%c] Receive using buffer %d",
                 thr_id == THREAD_CURSOR ? 'C' : 'F', idx);
    lp_comp_info * ctx    = mbuf->get_context(tag, idx);
    uint64_t       off    = mbuf->get_offset(tag, idx);
    tcm_mem *      mem    = mbuf->get_mem();
    size_t         maxlen = mbuf->get_slot_size(tag);
    ret                   = ep->recv(*mem, peer, ctx->fctx, off, maxlen);
    if (ret == -FI_EAGAIN) {
      mbuf->unlock(tag, idx);
      break;
    }
    if (ret < 0) {
      mbuf->unlock(tag, idx);
      lp_log_debug("[%c] Recv post error: %s",
                   thr_id == THREAD_CURSOR ? 'C' : 'F', fi_strerror(-ret));
    }
  }
}

int common_poll_cq(thread_id thr_id, fi_cq_data_entry * de,
                   fi_cq_err_entry * err) {
  shared_ptr<tcm_cq> cq = 0;
  switch (thr_id) {
    case THREAD_CURSOR: cq = state.resrc->ep_msg->get_cq().lock(); break;
    case THREAD_FRAME: cq = state.resrc->ep_frame->get_cq().lock(); break;
    default:
      assert(false && "Invalid thread number!");
      throw tcm_exception(EINVAL, _LP_FNAME_, __LINE__, "Bad thread number");
  }
  ssize_t ret = cq->poll(de, err, 1, nullptr, 0);
  cq          = 0;
  switch (ret) {
    case 0:
    case -FI_EAGAIN: return 0;
    case 1: return 1;
    case -FI_EAVAIL:
      return -FI_EAVAIL;
    default: throw tcm_exception(ret, _LP_FNAME_, __LINE__, "Fabric error");
  }
}

int common_disconnect() {
  ssize_t ret;
  lp_log_debug("Sending disconnect");
  state.ctrl |= S_EXIT;

  // Lock buffer
  int fidx = f_state.mbuf->lock(TAG_MSG_TX);
  if (fidx < 0)
    return -EAGAIN;

  int cidx = c_state.mbuf->lock(TAG_MSG_TX);
  if (cidx < 0) {
    f_state.mbuf->unlock(TAG_MSG_TX, fidx);
    return -EAGAIN;
  }

  // Prepare message
  void * fbuf = f_state.mbuf->get_buffer(TAG_MSG_TX, fidx);
  void * cbuf = c_state.mbuf->get_buffer(TAG_MSG_TX, cidx);

  NFRState * fst = static_cast<NFRState *>(fbuf);
  NFRState * cst = static_cast<NFRState *>(cbuf);

  fst->header = NFRHeaderCreate(NFR_MSG_STATE);
  fst->state  = NFR_STATE_DISCONNECT;
  cst->header = NFRHeaderCreate(NFR_MSG_STATE);
  cst->state  = NFR_STATE_DISCONNECT;

  tcm_mem * fmem = f_state.mbuf->get_mem();
  tcm_mem * cmem = c_state.mbuf->get_mem();

  uint64_t foff = f_state.mbuf->get_offset(TAG_MSG_TX, fidx);
  uint64_t coff = c_state.mbuf->get_offset(TAG_MSG_TX, cidx);

  lp_comp_info * fctx = f_state.mbuf->get_context(TAG_MSG_TX, fidx);
  lp_comp_info * cctx = c_state.mbuf->get_context(TAG_MSG_TX, cidx);

  // Post send
  ret = state.resrc->ep_msg->send(*fmem, state.resrc->peer_msg, fctx, foff,
                                  sizeof(NFRState));
  if (ret < 0) {
    f_state.mbuf->unlock(TAG_MSG_TX, fidx);
    c_state.mbuf->unlock(TAG_MSG_TX, cidx);
    return ret;
  }

  ret = state.resrc->ep_frame->send(*cmem, state.resrc->peer_frame, cctx, coff,
                                    sizeof(NFRState));
  if (ret < 0) {
    f_state.mbuf->unlock(TAG_MSG_TX, fidx);
  }

  return ret;
}