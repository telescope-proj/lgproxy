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

#ifndef _LP_SOURCE_H
#define _LP_SOURCE_H

#include "common/framebuffer.h"

#include "lp_log.h"
#include "lp_types.h"

#include <getopt.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>

/* Copy of the KVMFRFrame structure, but without damage rectangles */
typedef struct {
    uint32_t        frameSerial; 
    FrameType       type;        
    uint32_t        screenWidth; 
    uint32_t        screenHeight;
    uint32_t        frameWidth;  
    uint32_t        frameHeight; 
    FrameRotation   rotation;    
    uint32_t        stride;      
    uint32_t        pitch;       
    uint32_t        offset;      
    KVMFRFrameFlags flags;       
} lp_kvmfr;

/* Queues to store incoming and outgoing message details */

static inline void kvmfrframe_to_lp_kvmfr(KVMFRFrame * frame, lp_kvmfr * out)
{
    out->frameSerial  = frame->frameSerial;
    out->type         = frame->type;
    out->screenWidth  = frame->screenWidth;
    out->screenHeight = frame->screenHeight;
    out->frameWidth   = frame->frameWidth;
    out->frameHeight  = frame->frameHeight;
    out->rotation     = frame->rotation;
    out->stride       = frame->stride;
    out->pitch        = frame->pitch;
    out->offset       = frame->offset;
    out->flags        = frame->flags;
}

static inline void kvmfrframe_to_lp_msg_kvmfr(KVMFRFrame * frame,
                                              lp_msg_kvmfr * out)
{
    out->hdr.id = LP_MSG_KVMFR;
    out->req_id = 0;
    out->fmt    = frame->type;
    out->rot    = frame->rotation;
    out->fid    = frame->frameSerial;
    out->width  = frame->frameWidth;
    out->height = frame->frameHeight;
    out->stride = frame->stride;
    out->pitch  = frame->pitch;
    out->flags  = frame->flags;
}

static inline void kvmfrcursor_to_lp_msg_cursor(KVMFRCursor * cursor,
                                                uint32_t flags,
                                                lp_msg_cursor * out)
{
    out->hdr.id = LP_MSG_CURSOR;
    out->fmt = cursor->type;
    out->flags = flags;
    out->pos_x = cursor->x;
    out->pos_y = cursor->y;
    out->hot_x = cursor->hx;
    out->hot_y = cursor->hy;
    out->width = cursor->width;
    out->height = cursor->height;
    out->pitch = cursor->pitch;
}

#endif