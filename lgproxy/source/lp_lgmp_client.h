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

#ifndef _LP_LGMP_CLIENT_H_
#define _LP_LGMP_CLIENT_H_

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

#include "common/framebuffer.h"
#include "common/KVMFR.h"
#include "lgmp/client.h"

#include "lp_types.h"
#include "lp_utils.h"
#include "tcm_time.h"
#include "lp_log.h"

#include "lp_version.h"
#if LP_KVMFR_SUPPORTED_VER != KVMFR_VERSION
    #error KVMFR version mismatch!
#endif

typedef struct {
    union {
        KVMFRFrame  * frame;
        KVMFRCursor * cursor;
        void        * raw;
    };
    size_t       size;
    uint32_t     flags;
} lp_lgmp_msg;

typedef struct {
    PLGMPClient         cli;
    void              * mem;
    struct fid_mr     * mr;
    size_t              mem_size;
    PLGMPClientQueue    frame_q;
    PLGMPClientQueue    ptr_q;
    void              * udata;
    size_t              udata_size;
} lp_lgmp_client_ctx;

int lp_init_lgmp_client(const char * path, int * fd_out, void ** mem, 
                        size_t * size, PLGMPClient * cli);

int lp_enable_lgmp_client(PLGMPClient client, tcm_time * timeout, 
                          void ** data_out, size_t * data_size);

int lp_subscribe_lgmp_client(PLGMPClient client, tcm_time * timeout,
                             PLGMPClientQueue * frame_q,
                             PLGMPClientQueue * ptr_q);

int lp_create_lgmp_client(const char * path, tcm_time * timeout, 
                          lp_lgmp_client_ctx ** ctx_out);

int lp_resub_client(PLGMPClient client, PLGMPClientQueue * q, uint32_t q_id, 
                    tcm_time * timeout);

int lp_request_lgmp_any(lp_lgmp_client_ctx * ctx, LGMPMessage * out, 
                        tcm_time * timeout);

int lp_request_lgmp_frame(PLGMPClient client, PLGMPClientQueue * frame_q, 
                          lp_lgmp_msg * out,
                          tcm_time * timeout);

int lp_request_lgmp_cursor(PLGMPClient client, PLGMPClientQueue * ptr_q,
                           lp_lgmp_msg * out,
                           void * shape_out, tcm_time * timeout);

#endif