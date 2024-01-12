#include "common.hpp"
#include "cursor.hpp"
#include "frame.hpp"
#include "profiler.hpp"

using std::make_shared;
using std::shared_ptr;
using std::unique_ptr;
using std::weak_ptr;

#include "lg_build_version.h"

// Cursor thread: Supporting data types

struct msg_container {
  char     msg[LGMP_MSGS_SIZE];
  uint32_t udata;
  int8_t   index;
};

t_state_frame  f_state; // Shared global frame state
t_state_cursor c_state; // Shared global cursor state
shared_state   state;

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
  tcm__log_set_level(TCM__LOG_TRACE);
  signal(SIGINT, exit_handler);

  shared_ptr<lp_source_opts>   opts   = 0;
  shared_ptr<lp_lgmp_client_q> q      = 0;
  shared_ptr<tcm_beacon>       beacon = 0;
  shared_ptr<lp_shmem>         shm    = 0;
  shared_ptr<tcm_mem>          mbuf   = 0;
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
      std::string desc = e.full_desc();
      lp_log_error("%s", desc.c_str());
      lp_log_error("For help, run %s -h", argv[0]);
    }
    return 1;
  }

  pthread_t thr_profiler;
  if (opts->profiler) {
    state.profiler_lock    = PROFILER_STOP;
    state.core_state_str   = "Initializing";
    state.cursor_state_str = "Not connected";
    state.frame_state_str  = "Not connected";
    ret = pthread_create(&thr_profiler, NULL, profiler_thread, NULL);
    if (ret != 0) {
      lp_log_error("Failed to create profiler thread: %s", strerror(ret));
      errno = ret;
      goto err_cleanup;
    }
  }

  while (1) {

    state.ctrl             = 0;
    state.profiler_lock    = PROFILER_STOP;
    state.core_state_str   = "Waiting for client";
    state.cursor_state_str = "Not connected";
    state.frame_state_str  = "Not connected";
    lp_log_info("Ready to accept client");

    /* Accept a new client */

    NFRServerOpts s_opts;
    memset(&s_opts, 0, sizeof(s_opts));
    s_opts.api_version = opts->fabric_version;
    s_opts.build_ver   = LG_BUILD_VERSION;
    s_opts.src_addr    = opts->src_addr;
    s_opts.src_port    = opts->src_port;
    s_opts.timeout_ms  = opts->timeout;
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
      switch (ret) {
        case -EINTR:
        case -EAGAIN:
        case -ETIMEDOUT:
        case -EPROTO: flag = 1; break;
        default:
          lp_log_info("Connection failed: %s", fi_strerror(-ret));
          return 1;
      }
    }

    if (flag || exit_flag)
      break;

    assert(res.ep_frame.get());
    assert(res.ep_msg.get());
    assert(res.peer_frame != FI_ADDR_UNSPEC);
    assert(res.peer_msg != FI_ADDR_UNSPEC);

    // Register IVSHMEM region
    shared_ptr<lp_rdma_shmem> rshm =
        make_shared<lp_rdma_shmem>(res.fabric, shm);

    // Reset the initial conditions for the two threads

    pthread_mutex_t start_lock;
    pthread_mutex_init(&start_lock, NULL);
    pthread_mutex_lock(&start_lock);

    // Allocate message buffers

    errno = ENOMEM;

    c_state.mbuf.reset(new lp_mbuf(
        res.fabric,
        (1024 * 64) + (POINTER_SHAPE_BUFFERS * MAX_POINTER_SIZE_ALIGN)));
    if (!c_state.mbuf->alloc(TAG_MSG_RX, 1024, 32)) {
      lp_log_fatal("Memory allocation failed");
      goto err_cleanup;
    }
    if (!c_state.mbuf->alloc(TAG_MSG_TX, 1024, 32)) {
      lp_log_fatal("Memory allocation failed");
      goto err_cleanup;
    }
    if (!c_state.mbuf->alloc(TAG_CURSOR_SHAPE, MAX_POINTER_SIZE_ALIGN,
                             POINTER_SHAPE_BUFFERS)) {
      lp_log_fatal("Memory allocation failed");
      goto err_cleanup;
    }

    f_state.mbuf.reset(new lp_mbuf(res.fabric, 1024 * 16));
    if (!f_state.mbuf->alloc(TAG_FRAME_RX, 1024, 8)) {
      lp_log_fatal("Memory allocation failed");
      goto err_cleanup;
    }
    if (!f_state.mbuf->alloc(TAG_FRAME_TX, 1024, 8)) {
      lp_log_fatal("Memory allocation failed");
      goto err_cleanup;
    }
    if (!f_state.mbuf->alloc(TAG_FRAME_WRITE, 0, LGMP_Q_FRAME_LEN)) {
      lp_log_fatal("Memory allocation failed");
      goto err_cleanup;
    }
    if (!f_state.mbuf->alloc_extra(TAG_FRAME_WRITE, sizeof(frame_context))) {
      lp_log_fatal("Memory allocation failed");
      goto err_cleanup;
    }
    if (!c_state.mbuf->alloc_extra(TAG_CURSOR_SHAPE, sizeof(cursor_context))) {
      lp_log_fatal("Memory allocation failed");
      goto err_cleanup;
    }

    state.ctrl          = 0;
    state.lgmp_interval = opts->interval;
    state.lgmp_timeout  = opts->timeout;
    state.net_interval  = opts->interval;
    state.net_timeout   = opts->timeout;
    state.rshm          = rshm;
    state.start_lock    = &start_lock;
    state.resrc         = &res;

    // Start threads

    state.profiler_lock = PROFILER_START;
    pthread_t thr_cursor, thr_frame;

    ret = pthread_create(&thr_cursor, NULL, cursor_thread, NULL);
    if (ret != 0) {
      lp_log_error("Failed to create cursor thread: %s", strerror(ret));
      errno = ret;
      goto err_cleanup;
    }

    ret = pthread_create(&thr_frame, NULL, frame_thread, NULL);
    if (ret != 0) {
      lp_log_error("Failed to create frame thread: %s", strerror(ret));
      errno = ret;
      goto err_cleanup;
    }

    void * ret_cursor = 0;
    void * ret_frame  = 0;

    pthread_join(thr_frame, &ret_frame);
    pthread_join(thr_cursor, &ret_cursor);

    // Cleanup

    c_state.mbuf = 0;
    c_state.lgmp = 0;

    f_state.mbuf = 0;
    f_state.lgmp = 0;

    res.ep_frame   = 0;
    res.ep_msg     = 0;
    res.fabric     = 0;
    res.peer_frame = FI_ADDR_UNSPEC;
    res.peer_msg   = FI_ADDR_UNSPEC;

    if (opts->once) {
      lp_log_info("Run ended");
      break;
    }

    if (exit_flag) {
      lp_log_info("Exiting");
      break;
    }
  }

  state.profiler_lock = PROFILER_EXIT;
  pthread_join(thr_profiler, NULL);
  return 0;

err_cleanup:
  if (state.resrc) {
    state.resrc->ep_frame = 0;
    state.resrc->ep_msg   = 0;
    state.resrc->fabric   = 0;
  }

  state.rshm = 0;

  c_state.mbuf = 0;
  c_state.lgmp = 0;

  f_state.mbuf = 0;
  f_state.lgmp = 0;

  return errno;
}