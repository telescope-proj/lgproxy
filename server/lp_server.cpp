#include "lp_config.h"
#include "lp_utils.h"
#include "tcm_conn.h"
#include "tcm_errno.h"
#include "tcm_fabric.h"
#include <memory>
#include <pthread.h>
#include <queue>
#include <semaphore.h>
#include <signal.h>
#include <thread>

#include "lg_build_version.h"
#include "lp_build_version.h"

#include "lp_lgmp_client.hpp"
#include "nfr_server.hpp"
#include "nfr_util.hpp"

using std::make_shared;
using std::shared_ptr;
using std::unique_ptr;
using std::weak_ptr;

/* Exit exception */

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

// Cursor thread: Supporting data types

class cursor_shapes {

  uint32_t            max_cur_size;
  uint8_t             n;
  uint8_t *           states;
  unique_ptr<tcm_mem> mem;

public:
  cursor_shapes(shared_ptr<tcm_fabric> fabric, uint32_t max_cursor_size,
                uint8_t n) {
    this->mem.reset(new tcm_mem(fabric, max_cursor_size * n));
    this->states = static_cast<uint8_t *>(calloc(n, 1));
    if (!this->states)
      throw tcm_exception(ENOMEM, __FILE__, __LINE__,
                          "Failed to allocate cursor shape buffers");
    this->max_cur_size = max_cursor_size;
    this->n            = n;
  }

  void * claim() {
    for (uint8_t i = 0; i < n; i++) {
      if (this->states[i] == 0) {
        this->states[i] = 1;
        return (void *) ((uint8_t *) **this->mem + max_cur_size * i);
      }
    }
    return 0;
  }

  uint64_t get_rdma_resource(void * ptr, tcm_mem ** out) {
    if (ptr < **this->mem ||
        ptr >= (void *) ((uint8_t *) **this->mem + max_cur_size * n))
      throw tcm_exception(EINVAL, __FILE__, __LINE__, "Invalid buffer");
    uintptr_t off = (uintptr_t) ptr - (uintptr_t) (**this->mem);
    *out          = this->mem.get();
    return off;
  }

  void release(void * ptr) {
    if (!ptr)
      return;
    if (ptr < **this->mem ||
        ptr >= (void *) ((uint8_t *) (**this->mem) + max_cur_size * n))
      throw tcm_exception(EINVAL, __FILE__, __LINE__,
                          "Invalid cursor shape buffer release");
    uint8_t i =
        (uint8_t) ((uintptr_t) ptr - (uintptr_t) (**this->mem)) / max_cur_size;
    this->states[i] = 0;
  }

  ~cursor_shapes() {
    free(this->states);
    this->max_cur_size = 0;
    this->n            = 0;
  }
};

struct cursor_upd {
  int16_t         x, y;      // Position x, y
  int16_t         hx, hy;    // Hotspot x,y
  uint32_t        flags;     // KVMFR flags
  void *          buffer;    // Cursor shape buffer
  uint16_t        w, h;      // Cursor shape buffer width/height
  uint32_t        rb;        // Row bytes
  cursor_shapes * allocator; // Buffer owner
};

// Frame thread: Supporting data types

struct frame_upd {
  uint32_t      w, h;           // Width, height
  uint32_t      row_bytes;      // Bytes per row including padding
  uint32_t      flags;          // KVMFR flags
  FrameType     frame_type;     // KVMFRR frame type
  FrameRotation frame_rotation; // KVMFR frame rotation
};

// Common: Supporting data types

struct remote_buffer {
  uint64_t               base;
  uint64_t               rkey;
  uint64_t               maxlen;
  uint64_t               size;
  std::vector<NFROffset> offsets;
  std::vector<uint8_t>   used;

  void clear() {
    base   = 0;
    rkey   = 0;
    maxlen = 0;
    offsets.clear();
    used.clear();
  }

  bool import(NFRClientFrameBuf * b, uint32_t size) {
    int n_off = (size - sizeof(*b)) / sizeof(NFROffset);
    if (n_off <= 0 || n_off >= 127) {
      lp_log_error("Malformed frame buffer message");
      return false;
    }
    uint64_t max_off = 0;
    this->offsets.reserve(n_off);
    this->used.reserve(n_off);
    this->base   = b->base;
    this->rkey   = b->key;
    this->maxlen = b->maxlen;
    for (int i = 0; i < n_off; i++) {
      lp_log_debug("Offset %d = %lu", i, b->offsets[i]);
      if (b->offsets[i] > max_off)
        max_off = b->offsets[i];
      this->offsets.push_back(b->offsets[i]);
      this->used.push_back(0);
    }
    this->size = max_off + b->maxlen;
    return true;
  }

  bool import(NFRClientCursorBuf * b, uint32_t size) {
    int n_off = (size - sizeof(*b)) / sizeof(NFROffset);
    if (n_off <= 0 || n_off >= 127) {
      lp_log_error("Malformed cursor buffer message");
      return false;
    }
    uint64_t max_off = 0;
    this->offsets.reserve(n_off);
    this->used.reserve(n_off);
    this->base   = b->base;
    this->rkey   = b->key;
    this->maxlen = b->maxlen;
    for (int i = 0; i < n_off; i++) {
      lp_log_debug("Offset %d = %lu", i, b->offsets[i]);
      if (b->offsets[i] > max_off)
        max_off = b->offsets[i];
      this->offsets.push_back(b->offsets[i]);
      this->used.push_back(0);
    }
    this->size = max_off + b->maxlen;
    return true;
  }

  bool reclaim(int8_t * indexes, int n) {
    if (n < 0)
      return false;
    for (uint32_t i = 0; i < (uint32_t) n; i++) {
      if (indexes[i] < 0)
        continue;
      if (indexes[i] > (int8_t) this->offsets.size()) {
        lp_log_error("Index %d exceeds %d provided buffers", indexes[i],
                     this->offsets.size());
        return false;
      }
      if (this->used[indexes[i]] == 0) {
        lp_log_warn("Freeing already freed buffer %d!", indexes[i]);
        assert(false && "Peer performed double free on buffer");
      }
      this->used[i] = 0;
    }
    return true;
  }
};

enum completion_type {
  CONTEXT_RX,
  CONTEXT_TX,
  CONTEXT_WRITE
};

struct completion_info {
  /* For providers that require fi_context/fi_context2, they will overwrite the
   * first 4/8 bytes of the provided context, so this space is reserved */
  fi_context2     fctx;
  completion_type type;       // Type of the completion
  uint8_t         waiting;    // Whether this completion slot is in use
  int16_t         index;      // Transmit/Receive buffer index
  int16_t         remote_idx; // RDMA Write: remote buffer index
  uint8_t         batch;      // RDMA Write: Queued with a send
  union {
    struct cursor_upd c_upd; // Cursor data
    struct frame_upd  f_upd; // Frame data
  } ext;
};

enum thread_id {
  THREAD_CURSOR,
  THREAD_FRAME
};

// Frame thread: Global state

struct t_state_frame {
  int64_t                      lgmp_timeout;
  int64_t                      lgmp_interval;
  int64_t                      net_timeout;
  int64_t                      net_interval;
  shared_ptr<lp_rdma_shmem>    rshm;
  pthread_mutex_t *            start_lock;
  NFRServerResource *          resrc;
  uint64_t                     mbuf_size;
  uint64_t                     max_msg_size;
  uint64_t                     slots;
  unique_ptr<tcm_mem>          rx_buf;
  unique_ptr<tcm_mem>          tx_buf;
  completion_info *            tx_comp;
  completion_info *            rx_comp;
  remote_buffer                r_frame_buf;
  unique_ptr<lp_lgmp_client_q> lgmp;
  std::vector<uint8_t>         tx_states;
  std::vector<uint8_t>         rx_states;
  bool                         subbed;
  bool                         pause_req;
  bool                         pause_local;
  bool wait_fb; // Waiting for client framebuffer metadata
};

t_state_frame f_state; // Shared global frame state

// Cursor thread: Global state

struct t_state_cursor {
  int64_t                      lgmp_timeout;  // LGMP session timeout (ms)
  int64_t                      lgmp_interval; // LGMP polling interval (us)
  int64_t                      net_timeout;   // Network session timeout (ms)
  int64_t                      net_interval;  // Network polling interval (us)
  shared_ptr<lp_rdma_shmem>    rshm;          // Shared memory file
  pthread_mutex_t *            start_lock;    // Frame channel lock
  NFRServerResource *          resrc;         // NetFR resources
  uint64_t                     mbuf_size;     // Message buffer size
  uint64_t                     max_msg_size;  // Max message size
  uint64_t                     slots;         // Number of message slots
  unique_ptr<tcm_mem>          rx_buf;        // Receive bufferr
  unique_ptr<tcm_mem>          tx_buf;        // Send buffer
  completion_info *            tx_comp;       // Transmit completions
  completion_info *            rx_comp;       // Receive completions
  remote_buffer                r_cur_buf;     // Remote cursor buffer metadata
  unique_ptr<lp_lgmp_client_q> lgmp;          // LGMP session
  std::queue<cursor_upd>       cursor_q;      // Unsent cursor update queue
  unique_ptr<cursor_shapes>    shapes;        // Cursor shape data buffers
  std::vector<uint8_t>         tx_states;     // Transmit buffer locks
  std::vector<uint8_t>         rx_states;     // Receive buffer locks
  bool                         subbed;        // LGMP subscribed
  bool                         pause_req;     // Pause requested by client
  bool                         pause_local;   // Pause due to no data available
};

t_state_cursor c_state; // Shared global cursor state

// Common: Signal handler

volatile int exit_flag = 0;

void exit_handler(int sig) {
  lp_log_info("Signal %d", sig);
  if (sig == SIGINT) {
    lp_log_info("Ctrl+C %d of 3, press thrice to force quit", ++exit_flag);
    if (exit_flag == 2) {
      lp_log_info("Force quitting");
      exit(EINTR);
    }
  }
}

// Common: Shared subroutines between frame and cursor threads

static void common_keep_lgmp_connected(thread_id thr_id) {
  lp_lgmp_client_q * q    = 0;
  uint32_t           q_id = (uint32_t) -1;
  bool *             sub  = 0;
  switch (thr_id) {
    case THREAD_CURSOR:
      q_id = LGMP_Q_POINTER;
      q    = c_state.lgmp.get();
      sub  = &c_state.subbed;
      if (!q) {
        c_state.lgmp.reset(new lp_lgmp_client_q(
            c_state.rshm->shm, c_state.lgmp_timeout, c_state.lgmp_interval));
      }
      break;
    case THREAD_FRAME:
      q_id = LGMP_Q_FRAME;
      q    = f_state.lgmp.get();
      sub  = &f_state.subbed;
      if (!q) {
        f_state.lgmp.reset(new lp_lgmp_client_q(
            c_state.rshm->shm, c_state.lgmp_timeout, c_state.lgmp_interval));
      }
      break;
    default:
      assert(false && "Invalid thread number!");
      throw tcm_exception(EINVAL, __FILE__, __LINE__, "Bad thread number");
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
        throw tcm_exception(ENOTSUP, __FILE__, __LINE__,
                            "Mismatched LGMP version!");
      case LGMP_OK:
      default: return;
    }
  }
  if (!*sub) {
    s = q->subscribe(q_id);
    if (s == LGMP_OK) {
      lp_log_debug("[T%d] Resubscribed to host", thr_id);
      *sub = true;
    }
  }
}

static void common_update_lgmp_state(thread_id thr_id, LGMP_STATUS retcode) {
  bool * sub = 0;
  switch (thr_id) {
    case THREAD_CURSOR: sub = &c_state.subbed; break;
    case THREAD_FRAME: sub = &f_state.subbed; break;
    default:
      assert(false && "Invalid thread number!");
      throw tcm_exception(EINVAL, __FILE__, __LINE__, "Bad thread number");
  }
  switch (retcode) {
    case LGMP_ERR_QUEUE_EMPTY:
    case LGMP_ERR_QUEUE_FULL:
    case LGMP_OK: return;
    case LGMP_ERR_INVALID_VERSION:
      throw tcm_exception(ENOTSUP, __FILE__, __LINE__,
                          "Mismatched LGMP version!");
    default: *sub = false; return;
  }
}

static lp_lgmp_msg common_get_lgmp_message(thread_id thr_id) {
  lp_lgmp_msg        msg;
  lp_lgmp_client_q * q = 0;
  switch (thr_id) {
    case THREAD_CURSOR: q = c_state.lgmp.get(); break;
    case THREAD_FRAME: q = f_state.lgmp.get(); break;
    default:
      assert(false && "Invalid thread number!");
      throw tcm_exception(EINVAL, __FILE__, __LINE__, "Bad thread number");
  }
  msg = q->get_msg();
  if (msg.size < 0) {
    common_update_lgmp_state(thr_id, (LGMP_STATUS) -msg.size);
  }
  return msg;
}

static void common_post_recv(thread_id thr_id) {
  completion_info * comps    = 0;
  uint64_t          slots    = 0;
  uint64_t          max_size = 0;
  tcm_endpoint *    ep       = 0;
  fi_addr_t         peer     = FI_ADDR_UNSPEC;
  tcm_mem *         mbuf     = 0;
  ssize_t           ret;
  switch (thr_id) {
    case THREAD_CURSOR:
      comps    = c_state.rx_comp;
      slots    = c_state.slots;
      max_size = c_state.max_msg_size;
      ep       = c_state.resrc->ep_msg.get();
      peer     = c_state.resrc->peer_msg;
      mbuf     = c_state.rx_buf.get();
      break;
    case THREAD_FRAME:
      comps    = f_state.rx_comp;
      slots    = f_state.slots;
      max_size = f_state.max_msg_size;
      ep       = f_state.resrc->ep_frame.get();
      peer     = f_state.resrc->peer_frame;
      mbuf     = f_state.rx_buf.get();
      break;
    default:
      assert(false && "Invalid thread number!");
      throw tcm_exception(EINVAL, __FILE__, __LINE__, "Bad thread number");
  }
  for (uint64_t i = 0; i < slots; i++) {
    if (comps[i].waiting) {
      comps[i].type  = CONTEXT_RX;
      comps[i].index = i;
      ret            = ep->recv(*mbuf, peer, &comps[i], max_size * i, max_size);
      if (ret < 0) {
        lp_log_error("[T%d] Failed to queue receive: %s", thr_id,
                     fi_strerror(-ret));
        throw tcm_exception(-ret, __FILE__, __LINE__, "Receive queue failed");
      }
      comps[i].waiting = 1;
    }
  }
}

static int common_poll_cq(thread_id thr_id, fi_cq_data_entry * de,
                          fi_cq_err_entry * err) {
  tcm_cq * cq = 0;
  switch (thr_id) {
    case THREAD_CURSOR:
      cq = c_state.resrc->ep_msg->get_cq().lock().get();
      break;
    case THREAD_FRAME:
      cq = f_state.resrc->ep_frame->get_cq().lock().get();
      break;
    default:
      assert(false && "Invalid thread number!");
      throw tcm_exception(EINVAL, __FILE__, __LINE__, "Bad thread number");
  }
  ssize_t ret = cq->poll(de, err, 1, nullptr, 0);
  switch (ret) {
    case 0:
    case -FI_EAGAIN: return 0;
    case 1: return 1;
    case -FI_EAVAIL:
      throw tcm_exception(err->err, __FILE__, __LINE__, "Fabric error");
    default: throw tcm_exception(ret, __FILE__, __LINE__, "Fabric error");
  }
}

// Frame thread: Incoming message processing

static int ft_process_in_setup(NFRClientFrameBuf * msg, uint64_t size) {
  if (size < sizeof(*msg))
    return -EBADMSG;
  if (!f_state.wait_fb)
    return -EBADMSG;
  if (!f_state.r_frame_buf.import(msg, (uint32_t) size))
    return -EBADMSG;
  return 0;
}

static int ft_process_in_ack(NFRClientAck * msg, uint64_t size) {
  if (size < sizeof(*msg))
    return -EBADMSG;
  if (msg->type != NFR_BUF_CURSOR_DATA)
    return -EBADMSG;
  int n_idx = size - sizeof(*msg);
  if (!f_state.r_frame_buf.reclaim(msg->indexes, n_idx))
    return -EBADMSG;
  return 0;
}

static int ft_process_in(fi_cq_data_entry * de) {
  completion_info * info = static_cast<completion_info *>(de->op_context);
  void *            buf =
      ((uint8_t *) **f_state.rx_buf) + f_state.max_msg_size * info->index;
  NFRHeader * hdr = static_cast<NFRHeader *>(buf);
  if (!nfrHeaderVerify(hdr, de->len))
    return -EBADMSG;

  int ret;
  switch (hdr->type) {
    case NFR_MSG_CLIENT_ACK:
      ret = ft_process_in_ack(static_cast<NFRClientAck *>(buf), de->len);
      break;
    case NFR_MSG_CLIENT_FRAME_BUF:
      ret = ft_process_in_setup(static_cast<NFRClientFrameBuf *>(buf), de->len);
      break;
    default: ret = -EBADMSG;
  }
  info->waiting = 0;
  return ret;
}

// Frame thread: Outgoing message processing

static int ft_process_out(fi_cq_data_entry * de) {
  completion_info * info = static_cast<completion_info *>(de->op_context);
  if (info->type != CONTEXT_TX)
    throw tcm_exception(EINVAL, __FILE__, __LINE__,
                        "Completion routed to incorrect handler");
  info->waiting = 0;
  return 0;
}

static int ft_process_write(fi_cq_data_entry * de) {
  completion_info *   info = static_cast<completion_info *>(de->op_context);
  NFRServerResource * nfr  = f_state.resrc;
  ssize_t             ret;
  if (info->type != CONTEXT_WRITE)
    throw tcm_exception(EINVAL, __FILE__, __LINE__,
                        "Completion routed to incorrect handler");
  if (info->remote_idx < 0 ||
      info->remote_idx >= (int8_t) f_state.r_frame_buf.offsets.size())
    throw tcm_exception(EINVAL, __FILE__, __LINE__, "Bad completion data");

  // Release the frame
  LGMP_STATUS s = f_state.lgmp->ack_msg();
  if (s != LGMP_OK) {
    common_update_lgmp_state(THREAD_FRAME, s);
  }

  info->waiting = 0;
  if (info->batch) {
    return 0;
  }

  // Send acknowledgement of successful RDMA write, reuse buffer
  NFRFrameMetadata * mtd = reinterpret_cast<NFRFrameMetadata *>(
      ((uint8_t *) **f_state.tx_buf) + f_state.max_msg_size * info->index);

  mtd->header         = NFRHeaderCreate(NFR_MSG_FRAME_METADATA);
  mtd->buffer         = info->remote_idx;
  mtd->width          = info->ext.f_upd.w;
  mtd->height         = info->ext.f_upd.h;
  mtd->flags          = info->ext.f_upd.flags;
  mtd->row_bytes      = info->ext.f_upd.row_bytes;
  mtd->frame_type     = (NFRFrameType) info->ext.f_upd.frame_type;
  mtd->frame_rotation = (NFRFrameRotation) info->ext.f_upd.frame_rotation;
  ret = nfr->ep_frame->send(*f_state.tx_buf, nfr->peer_frame, de->op_context,
                            f_state.max_msg_size * info->index, sizeof(*mtd));
  if (ret == 0)
    info->waiting = 1;
  return ret;
}

// Frame thread: Completion router

static int ft_process_completion(fi_cq_data_entry * de) {
  int               ret;
  completion_info * info = static_cast<completion_info *>(de->op_context);
  switch (info->type) {
    case CONTEXT_RX: ret = ft_process_in(de); break;
    case CONTEXT_TX: ret = ft_process_out(de); break;
    case CONTEXT_WRITE: ret = ft_process_write(de); break;
    default: ret = -EINVAL;
  }
  info->waiting = 0;
  return ret;
}

// Frame thread: LGMP incoming message processing

static int ft_process_in_lgmp(lp_lgmp_msg * msg) {
  // Unlike the cursor buffer, it wastes a significant amount of memory and time
  // to copy the frame to an intermediate buffer. The update speed and users'
  // latency sensitivity for frames is also significantly lower than that of
  // cursor data.
  KVMFRFrame *        f = msg->frame();
  ssize_t             ret;
  NFRServerResource * nfr = f_state.resrc;

  frame_upd upd;
  upd.w              = f->dataWidth;
  upd.h              = f->dataHeight;
  upd.row_bytes      = f->pitch;
  upd.frame_rotation = f->rotation;
  upd.frame_type     = f->type;
  upd.flags          = msg->udata;

  int8_t ridx = -1;
  for (int8_t i = 0; i < (int8_t) f_state.r_frame_buf.used.size(); i++) {
    if (!f_state.r_frame_buf.used[i])
      ridx = i;
  }
  if (ridx < 0) {
    lp_log_debug("Remote frame buffers full!");
    return -EAGAIN;
  }

  void *         tex = (void *) (((uint8_t *) f) + f->offset + FB_WP_SIZE);
  tcm_remote_mem rbuf;
  rbuf.addr   = f_state.r_frame_buf.base;
  rbuf.len    = f_state.r_frame_buf.size;
  rbuf.peer   = nfr->peer_frame;
  rbuf.raw    = false;
  rbuf.u.rkey = f_state.r_frame_buf.rkey;

  if (tex < **f_state.rshm->mem) {
    lp_log_debug("Invalid frame buffer!");
    throw tcm_exception(EINVAL, __FILE__, __LINE__,
                        "Frame buffer out of range");
  }
  uint64_t off = (uintptr_t) tex - (uintptr_t) * *f_state.rshm->mem;
  for (uint64_t i = 0; i < f_state.slots; i++) {
    if (f_state.tx_comp[i].waiting)
      continue;
    ret = nfr->ep_frame->rwrite(*f_state.rshm->mem, rbuf, &f_state.tx_comp[i],
                                off, f_state.r_frame_buf.offsets[ridx],
                                upd.row_bytes * upd.h);
    if (ret < 0) {
      lp_log_debug("Failed to perform send: %s", fi_strerror(-ret));
      return ret;
    }
  }
  return 0;
}

void * frame_thread(void * arg) {
  (void) arg;
  ssize_t ret;
  try {
    lp_lgmp_msg msg;
    // Wait for metadata thread to be ready
    pthread_mutex_lock(f_state.start_lock);

    while (1) {
      common_keep_lgmp_connected(THREAD_FRAME);
      common_post_recv(THREAD_FRAME);

      // Check completions
      fi_cq_data_entry de;
      fi_cq_err_entry  err;
      ret = common_poll_cq(THREAD_FRAME, &de, &err);
      if (ret == 1) {
        ret = ft_process_completion(&de);
        if (ret < 0) {
          lp_log_error("[C] Message error: %s", fi_strerror(-ret));
          return 0;
        }
      }

      // Get an LGMP message
      msg = common_get_lgmp_message(THREAD_FRAME);
      if (msg.size < 0)
        continue;

      // Process LGMP messages
      ret = ft_process_in_lgmp(&msg);
      if (ret < 0)
        continue;
    }
  } catch (tcm_exception & e) {
    char * d = e.full_desc();
    lp_log_error("%s", d);
    free(d);
    return 0;
  } catch (lp_exit & e) {
    return 0;
  }
  return 0;
}

// Cursor thread: Preliminary requirements

static int ct_gather_host_metadata(NFRHostMetadataConstructor & c) {
  lp_lgmp_client_q q(c_state.rshm->shm, c_state.lgmp_timeout,
                     c_state.lgmp_interval);
  q.bind_exit_flag(&exit_flag);
  LGMP_STATUS s = q.init();
  if (exit_flag)
    return -ECANCELED;
  if (s == LGMP_ERR_INVALID_VERSION)
    return -ENOTSUP;
  lp_kvmfr_udata_to_nfr(q.udata, q.udata_size, c);
  return 0;
}

// Cursor thread: Incoming message processing

static int ct_process_in_ack(NFRClientAck * msg, uint64_t size) {
  if (size < sizeof(*msg))
    return -EBADMSG;
  if (msg->type != NFR_BUF_CURSOR_DATA)
    return -EBADMSG;
  int n_idx = size - sizeof(*msg);
  if (!c_state.r_cur_buf.reclaim(msg->indexes, n_idx))
    return -EBADMSG;
  return 0;
}

static int ct_process_in_state(NFRState * msg, uint64_t size) {
  if (size < sizeof(*msg))
    return -EBADMSG;
  switch (msg->state) {
    case NFR_STATE_DISCONNECT: throw lp_exit(LP_EXIT_REMOTE);
    case NFR_STATE_KA: return 0;
    case NFR_STATE_PAUSE: c_state.pause_req = 1; return 0;
    case NFR_STATE_RESUME: c_state.pause_req = 0; return 1;
    default: return -EBADMSG;
  }
}

static int ct_process_in_align(NFRCursorAlign * msg, uint64_t size) {
  // todo
  (void) msg;
  (void) size;
  return 0;
}

static int ct_process_in(fi_cq_data_entry * de) {
  completion_info * info = static_cast<completion_info *>(de->op_context);
  void *            buf =
      ((uint8_t *) **c_state.rx_buf) + c_state.max_msg_size * info->index;
  NFRHeader * hdr = static_cast<NFRHeader *>(buf);
  if (!nfrHeaderVerify(hdr, de->len))
    return -EBADMSG;

  int ret;
  switch (hdr->type) {
    case NFR_MSG_CLIENT_ACK:
      ret = ct_process_in_ack(static_cast<NFRClientAck *>(buf), de->len);
      break;
    case NFR_MSG_STATE:
      ret = ct_process_in_state(static_cast<NFRState *>(buf), de->len);
      break;
    case NFR_MSG_CURSOR_ALIGN:
      ret = ct_process_in_align(static_cast<NFRCursorAlign *>(buf), de->len);
      break;
    default: ret = -EBADMSG;
  }
  info->waiting = 0;
  return ret;
}

// Cursor thread: Outgoing message processing

static int ct_process_out(fi_cq_data_entry * de) {
  completion_info * info = static_cast<completion_info *>(de->op_context);
  if (info->type != CONTEXT_WRITE)
    throw tcm_exception(EINVAL, __FILE__, __LINE__,
                        "Completion routed to incorrect handler");
  info->waiting = 0;
  return 0;
}

static int ct_process_write(fi_cq_data_entry * de) {
  completion_info *   info = static_cast<completion_info *>(de->op_context);
  NFRServerResource * nfr  = c_state.resrc;
  ssize_t             ret;
  if (info->type != CONTEXT_WRITE)
    throw tcm_exception(EINVAL, __FILE__, __LINE__,
                        "Completion routed to incorrect handler");
  if (info->remote_idx < 0 ||
      info->remote_idx >= (int16_t) c_state.r_cur_buf.offsets.size())
    throw tcm_exception(EINVAL, __FILE__, __LINE__, "Bad completion data");
  info->waiting = 0;

  // Send acknowledgement of successful RDMA write
  for (uint64_t i = 0; i < c_state.slots; i++) {
    if (c_state.tx_comp[i].waiting == 0) {
      NFRCursorMetadata * mtd = reinterpret_cast<NFRCursorMetadata *>(
          ((uint8_t *) **c_state.tx_buf) + c_state.max_msg_size * i);
      mtd->header    = NFRHeaderCreate(NFR_MSG_CURSOR_METADATA);
      mtd->buffer    = info->remote_idx;
      mtd->flags     = info->ext.c_upd.flags;
      mtd->x         = info->ext.c_upd.y;
      mtd->y         = info->ext.c_upd.y;
      mtd->hx        = info->ext.c_upd.hx;
      mtd->hy        = info->ext.c_upd.hy;
      mtd->row_bytes = info->ext.c_upd.rb;
      mtd->width     = info->ext.c_upd.w;
      mtd->height    = info->ext.c_upd.h;
      info->ext.c_upd.allocator->release(info->ext.c_upd.buffer);
      info->ext.c_upd.buffer = 0;
      ret =
          nfr->ep_msg->send(*c_state.tx_buf, nfr->peer_msg, &c_state.tx_comp[i],
                            c_state.max_msg_size * i, sizeof(*mtd));
      if (ret == 0)
        c_state.tx_comp[i].waiting = 1;
      return ret;
    }
  }
  return -EAGAIN;
}

// Cursor thread: Completion router

static int ct_process_completion(fi_cq_data_entry * de) {
  int               ret;
  completion_info * info = static_cast<completion_info *>(de->op_context);
  switch (info->type) {
    case CONTEXT_RX: ret = ct_process_in(de); break;
    case CONTEXT_TX: ret = ct_process_out(de); break;
    case CONTEXT_WRITE: ret = ct_process_write(de); break;
    default: ret = -EINVAL;
  }
  info->waiting = 0;
  return ret;
}

// Cursor thread: LGMP incoming message processing

static int ct_process_in_lgmp(lp_lgmp_msg * msg) {
  // We do not initiate the write from here, as network operations are
  // (relatively) slow. The cursor info is pushed into a local queue to clear
  // out the LGMP queue as quickly as possible, and another function processes
  // anything in the cursor queue.
  cursor_upd         upd;
  KVMFRCursor *      c = msg->cursor();
  lp_lgmp_client_q * q = c_state.lgmp.get();
  upd.x                = c->x;
  upd.y                = c->y;
  upd.hx               = c->hx;
  upd.hy               = c->hy;
  upd.rb               = c->pitch;
  upd.w                = c->width;
  upd.h                = c->height;
  upd.buffer           = 0;
  upd.allocator        = 0;
  if (msg->udata & CURSOR_FLAG_SHAPE) {
    void * buf = c_state.shapes->claim();
    if (!buf) {
      lp_log_warn("No available data buffers, cursor shape dropped!");
      lp_log_warn("Your network may be too congested!");
    } else {
      upd.buffer    = buf;
      upd.allocator = c_state.shapes.get();
      void * tex    = (void *) (c + 1);
      memcpy(buf, tex, c->pitch * c->height);
    }
    LGMP_STATUS s = q->ack_msg();
    if (s != LGMP_OK) {
      lp_log_warn("LGMP queue ack failed: %s", lgmpStatusString(s));
      common_update_lgmp_state(THREAD_CURSOR, s);
      return -EAGAIN;
    }
    c_state.cursor_q.push(upd);
  }
  return 0;
}

// Cursor thread: Clear pending cursor messages

static int ct_flush_cursor_q() {
  NFRServerResource * nfr = c_state.resrc;
  ssize_t             ret;
  int                 n = 0;
  while (!c_state.cursor_q.empty()) {
    cursor_upd & upd = c_state.cursor_q.front();

    // If this message has texture data, find a free remote buffer
    int8_t ridx = -1;
    if (upd.buffer && (upd.flags & CURSOR_FLAG_SHAPE)) {
      for (int8_t i = 0; i < (int8_t) c_state.r_cur_buf.used.size(); i++) {
        if (!c_state.r_cur_buf.used[i]) {
          ridx = i;
        }
      }
    }
    if (ridx < 0) {
      lp_log_debug("Remote cursor texture buffers full!");
      break;
    }

    int idx = -1;
    for (uint64_t i = 0; i < c_state.slots; i++) {
      if (!c_state.tx_comp[i].waiting) {
        idx = i;
        break;
      }
    }
    if (idx < 0) {
      lp_log_debug("No local buffers available!");
      break;
    }

    tcm_remote_mem rbuf;
    rbuf.addr     = c_state.r_cur_buf.base;
    rbuf.len      = c_state.r_cur_buf.size;
    rbuf.peer     = nfr->peer_msg;
    rbuf.raw      = false;
    rbuf.u.rkey   = c_state.r_cur_buf.rkey;
    tcm_mem * mem = 0;
    uint64_t  off = upd.allocator->get_rdma_resource(upd.buffer, &mem);

    ret = nfr->ep_msg->rwrite(*mem, rbuf, &c_state.tx_comp[idx], off,
                              c_state.r_cur_buf.maxlen * ridx, upd.rb * upd.h);
    if (ret < 0) {
      lp_log_debug("Failed to perform RDMA write: %s", fi_strerror(-ret));
      return ret;
    }
    c_state.r_cur_buf.used[ridx]    = 1;
    c_state.tx_comp[idx].waiting    = 1;
    c_state.tx_comp[idx].remote_idx = ridx;
    c_state.tx_comp[idx].ext.c_upd  = upd;
    c_state.tx_comp[idx].index      = idx;
    c_state.tx_comp[idx].type       = CONTEXT_WRITE;
    c_state.tx_comp[idx].batch      = 1;
    n++;
  }
  if (n == 0 && !c_state.cursor_q.empty())
    return -EAGAIN;
  return n;
}

// Cursor main thread

void * cursor_thread(void * arg) {
  (void) arg;
  ssize_t             ret;
  NFRServerResource * nfr = c_state.resrc;
  // Collect metadata from the LGMP host. We start an LGMP session, collect
  // the metadata, and immediately close it, and send the metadata over the
  // network to the peer.
  try {
    char name[256];
    snprintf(
        name, 256, "LGProxy %d.%d.%d (%s), Looking Glass %s, Libfabric %d.%d",
        LP_VERSION_MAJOR, LP_VERSION_MINOR, LP_VERSION_PATCH, LP_BUILD_VERSION,
        LG_BUILD_VERSION, FI_MAJOR(fi_version()), FI_MINOR(fi_version()));
    uint16_t name_len = strnlen(name, 256);

    NFRHostMetadataConstructor c(**c_state.tx_buf, c_state.tx_buf->get_len());
    uint8_t                    proxied = 1;
    c.addField(NFR_F_NAME, name, name_len);
    c.addField(NFR_F_EXT_PROXIED, &proxied);
    ret = ct_gather_host_metadata(c);
    switch (ret) {
      case ECANCELED: lp_log_info("[C] Exit signal received"); return 0;
      case ENOTSUP: lp_log_fatal("LGMP version mismatch"); return 0;
      case 0: break;
      default: assert(false && "Unexpected state!");
    }
    ret = nfr->ep_msg->ssend(*c_state.tx_buf, nfr->peer_msg, 0, c.getUsed());
    if (ret < 0) {
      lp_log_error("Failed to send data: %s", fi_strerror(-ret));
      return 0;
    }
  } catch (tcm_exception & e) {
    char * d = e.full_desc();
    lp_log_error("%s", d);
    free(d);
    return 0;
  }

  // Before we can relay cursor data, the client must send cursor texture
  // buffers to the server. Unlike frame data these cursor texture buffers are
  // permanently allocated for the duration of the session and cannot be
  // changed.
  try {
    while (1) {
      bool flag = false;
      ret       = nfr->ep_msg->srecv(*c_state.rx_buf, nfr->peer_msg, 0,
                                     c_state.max_msg_size);
      if (ret < 0) {
        lp_log_error("[C] Failed to receive metadata: %s", fi_strerror(-ret));
        return 0;
      }
      NFRHeader * hdr = static_cast<NFRHeader *>(**c_state.rx_buf);
      if (!nfrHeaderVerify(**c_state.rx_buf, ret)) {
        lp_log_error("[C] Malformed message received!");
        return 0;
      }
      switch (hdr->type) {
        case NFR_MSG_STATE: {
          NFRState * state = static_cast<NFRState *>(**c_state.rx_buf);
          switch (state->state) {
            case NFR_STATE_KA: continue;
            case NFR_STATE_DISCONNECT:
              lp_log_info("[C] Client sent disconnect message");
              return 0;
            default:
              lp_log_error("[C] Client sent invalid state value %d",
                           state->state);
              return 0;
          }
        }
        case NFR_MSG_CLIENT_CURSOR_BUF: {
          NFRClientCursorBuf * b =
              static_cast<NFRClientCursorBuf *>(**c_state.rx_buf);
          if (!c_state.r_cur_buf.import(b, ret)) {
            lp_log_error("Failed to import cursor metadata buffers");
            return 0;
          }
          flag = true;
          break;
        }
        default:
          lp_log_error("[C] Unexpected message type %d at this "
                       "connection stage",
                       hdr->type);
          return 0;
      }
      if (flag)
        break;
    }
  } catch (tcm_exception & e) {
    char * d = e.full_desc();
    lp_log_error("%s", d);
    free(d);
    return 0;
  }

  // Main event loop
  try {
    lp_lgmp_msg msg;
    while (1) {
      common_keep_lgmp_connected(THREAD_CURSOR);
      common_post_recv(THREAD_CURSOR);

      if (!c_state.lgmp->connected()) {
        continue;
      }

      // Poll for a completion (both TX and RX)
      fi_cq_data_entry de;
      fi_cq_err_entry  err;
      ret = common_poll_cq(THREAD_CURSOR, &de, &err);
      if (ret == 1) {
        ret = ct_process_completion(&de);
        if (ret < 0) {
          lp_log_error("[C] Message error: %s", fi_strerror(-ret));
          return 0;
        }
      }

      // Get an LGMP message
      msg = common_get_lgmp_message(THREAD_CURSOR);
      if (msg.size < 0)
        continue;

      // Process LGMP messages
      ct_process_in_lgmp(&msg);

      // Send cursor data
      ct_flush_cursor_q();
    }

  } catch (tcm_exception & e) {
    char * d = e.full_desc();
    lp_log_error("%s", d);
    free(d);
    return 0;
  } catch (lp_exit & e) {
    lp_log_info("[C] User requested exit");
    return 0;
  }
}

int main(int argc, char ** argv) {
  if (getuid() == 0 || getuid() != geteuid()) {
    printf("===============================================================\n"
           "Do not run LGProxy as the root user or with setuid!            \n"
           "If you are running into permission issues, it is highly likely \n"
           "that you have missed a step in the instructions. LGProxy never \n"
           "requires root permissions; please check the instructions for   \n"
           "detailed troubleshooting steps to fix any issues.              \n"
           "                                                               \n"
           "Quick troubleshooting steps:                                   \n"
           " - Is the locked memory limit (ulimit -l) high enough?         \n"
           " - Are the permissions of the shared memory file correct?      \n"
           " - Is the RDMA subsystem configured correctly?                 \n"
           "==============================================================="
           "\n");
    return 1;
  }

  lp_set_log_level();
  signal(SIGINT, exit_handler);

  shared_ptr<tcm_fabric>       fab      = 0;
  shared_ptr<tcm_endpoint>     d_ep     = 0;
  shared_ptr<tcm_endpoint>     f_ep     = 0;
  shared_ptr<lp_source_opts>   opts     = 0;
  shared_ptr<lp_lgmp_client_q> q        = 0;
  shared_ptr<tcm_beacon>       beacon   = 0;
  shared_ptr<lp_shmem>         shm      = 0;
  shared_ptr<tcm_mem>          mbuf     = 0;
  fi_addr_t                    peers[2] = {FI_ADDR_UNSPEC, FI_ADDR_UNSPEC};
  ssize_t                      ret;

  bool hp = 0;

  try {
    lp_log_info("Parsing options");
    opts = make_shared<lp_source_opts>(argc, argv);
    shm  = make_shared<lp_shmem>(opts->file, 0, hp);
    lp_log_info("Initializing TCM");
    ret = tcm_init(nullptr);
    if (ret < 0)
      throw tcm_exception(-ret, __FILE__, __LINE__, "TCM init failed");
  } catch (tcm_exception & e) {
    if (e.return_code() == EAGAIN) {
      lp_print_usage(1);
    } else {
      char * d = e.full_desc();
      lp_log_error("%s", d);
      free(d);
      lp_log_error("For help, run %s -h", argv[0]);
    }
    return 1;
  }

  while (1) {

    /* Clean up previous client resources, if there was one */

    if (fab) {
      if (peers[0] != FI_ADDR_UNSPEC)
        fab->remove_peer(peers[0]);
      if (peers[1] != FI_ADDR_UNSPEC)
        fab->remove_peer(peers[1]);
    }
    d_ep = 0;
    mbuf = 0;
    fab  = 0;

    /* Accept a new client */

    NFRServerOpts s_opts;
    memset(&s_opts, 0, sizeof(s_opts));
    s_opts.api_version = opts->fabric_version;
    s_opts.build_ver   = LG_BUILD_VERSION;
    s_opts.src_addr    = opts->src_addr;
    s_opts.src_port    = opts->src_port;
    s_opts.timeout_ms  = 3000;
    s_opts.transport   = opts->transport;

    NFRServerResource res;
    res.ep_frame   = 0;
    res.ep_msg     = 0;
    res.fabric     = 0;
    res.peer_frame = 0;
    res.peer_msg   = 0;

    int flag = 0;
    ret      = NFRServerCreate(s_opts, res);
    if (ret < 0) {
      lp_log_info("Connection failed: %s", fi_strerror(-ret));
      switch (ret) {
        case -EAGAIN:
        case -ETIMEDOUT:
        case -EPROTO: flag = 1;
        default: continue;
      }
    }

    if (flag)
      break;

    // Register IVSHMEM region
    shared_ptr<lp_rdma_shmem> rshm =
        make_shared<lp_rdma_shmem>(res.fabric, shm);

    // Reset the initial conditions for the two threads

    pthread_mutex_t start_lock;
    pthread_mutex_init(&start_lock, NULL);
    pthread_mutex_lock(&start_lock);

    uint64_t mbuf_size    = 65536;
    uint64_t max_msg_size = 1024;
    uint64_t slots        = mbuf_size / max_msg_size;

    c_state.lgmp_timeout  = 1000;
    c_state.lgmp_interval = opts->interval;
    c_state.net_timeout   = opts->timeout;
    c_state.net_interval  = opts->interval;
    c_state.rshm          = rshm;
    c_state.start_lock    = &start_lock;
    c_state.resrc         = &res;
    c_state.mbuf_size     = 65536;
    c_state.max_msg_size  = 1024;
    c_state.slots         = c_state.mbuf_size / c_state.max_msg_size;
    c_state.rx_buf.reset(new tcm_mem(res.fabric, c_state.mbuf_size));
    c_state.tx_buf.reset(new tcm_mem(res.fabric, c_state.mbuf_size));
    c_state.tx_comp =
        (completion_info *) calloc(slots, sizeof(*c_state.tx_comp));
    c_state.rx_comp =
        (completion_info *) calloc(slots, sizeof(*c_state.rx_comp));
    c_state.r_cur_buf.clear();
    c_state.lgmp   = 0;
    c_state.shapes = 0;
    c_state.tx_states.clear();
    c_state.rx_states.clear();
    if (!c_state.tx_comp || !c_state.rx_comp) {
      lp_log_fatal("Failed to allocate memory");
      return 1;
    }

    f_state.lgmp_timeout  = 1000;
    f_state.lgmp_interval = opts->interval;
    f_state.net_timeout   = opts->timeout;
    f_state.net_interval  = opts->interval;
    f_state.rshm          = rshm;
    f_state.start_lock    = &start_lock;
    f_state.resrc         = &res;
    f_state.mbuf_size     = 65536;
    f_state.max_msg_size  = 1024;
    f_state.slots         = f_state.mbuf_size / f_state.max_msg_size;
    f_state.rx_buf.reset(new tcm_mem(res.fabric, f_state.mbuf_size));
    f_state.tx_buf.reset(new tcm_mem(res.fabric, f_state.mbuf_size));
    f_state.tx_comp =
        (completion_info *) calloc(slots, sizeof(*f_state.tx_comp));
    f_state.rx_comp =
        (completion_info *) calloc(slots, sizeof(*f_state.rx_comp));
    f_state.r_frame_buf.clear();
    f_state.lgmp = 0;
    f_state.tx_states.clear();
    f_state.rx_states.clear();
    if (!f_state.tx_comp || !f_state.rx_comp) {
      lp_log_fatal("Failed to allocate memory");
      return 1;
    }

    // Start threads

    pthread_t thr_cursor;
    pthread_t thr_frame;

    pthread_create(&thr_cursor, NULL, cursor_thread, NULL);
    pthread_create(&thr_frame, NULL, frame_thread, NULL);

    void * ret_cursor = 0;
    void * ret_frame  = 0;

    pthread_join(thr_frame, &ret_frame);
    pthread_join(thr_cursor, &ret_cursor);

    // Free manual allocations
    free(c_state.tx_comp);
    free(c_state.rx_comp);
    free(f_state.tx_comp);
    free(f_state.rx_comp);

    c_state.rshm  = 0;
    c_state.resrc = 0;
    c_state.rx_buf.reset();
    c_state.tx_buf.reset();
    c_state.tx_states.resize(0);
    c_state.rx_states.resize(0);
  }
}