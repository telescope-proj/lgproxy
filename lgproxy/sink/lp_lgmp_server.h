#ifndef _LP_LGMP_SERVER_H_
#define _LP_LGMP_SERVER_H_

#include <rdma/fabric.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "lp_types.h"

#include "module/kvmfr.h"
#include "common/KVMFR.h"
#include "lgmp/lgmp.h"

/* RDMA receive queue length */
#define RX_Q_LEN (LGMP_Q_POINTER_LEN + LGMP_Q_FRAME_LEN)

/* Largest metadata message size */
#define RX_Q_MAX_RECV (sizeof(lp_msg_cursor))

typedef struct {
    void            * mem;
    size_t          mem_size;
    struct fid_mr   * mr;
    int             fd, dma_fd;

    PLGMPHost       host;

    PLGMPHostQueue  frame_q;
    size_t          frame_q_pos;
    PLGMPMemory     frame_q_mem[LGMP_Q_FRAME_LEN];
    uint32_t        frame_udata[LGMP_Q_FRAME_LEN];
    
    PLGMPHostQueue  ptr_q;
    size_t          ptr_q_pos;
    PLGMPMemory     ptr_q_mem[LGMP_Q_POINTER_LEN];
    uint32_t        ptr_udata[LGMP_Q_POINTER_LEN];
    PLGMPMemory     ptr_shape_mem[POINTER_SHAPE_BUFFERS];
    uint32_t        ptr_shape_udata[POINTER_SHAPE_BUFFERS];
    size_t          ptr_shape_pos;
} lp_lgmp_server_ctx;


int lp_init_lgmp_host(lp_lgmp_server_ctx * ctx, KVMFRRecord_VMInfo * vm_info,
                      KVMFRRecord_OSInfo * os_info, uint16_t feature_flags,
                      size_t display_size);

int lp_init_mem_server(const char * path, int * fd_out, int * dma_fd_out, 
                       void ** mem, size_t * size);

#endif