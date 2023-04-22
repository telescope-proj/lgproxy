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