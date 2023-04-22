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

#ifndef _LP_MSG_H
#define _LP_MSG_H

#include <stdio.h>

#include "lp_types.h"

/*  The lower 16 bits of the 64-bit message tag are reserved for future use, to
    distinguish different messages of the same type */

// #define LP_TAG_MASK (0x0FFFF)
#define LP_TAG_MASK 0
#define LP_TAG_BASE (0x10000)

enum lp_fabric_tags {
    LP_TAG_ANY,
    /* Buffer and peer status */
    LP_TAG_STAT             = LP_TAG_BASE,
    /* System information */
    LP_TAG_SYS_INFO         = (LP_TAG_BASE),
    /* Frame metadata and texture data */
    LP_TAG_FRAME_INFO       = (LP_TAG_BASE * 2),
    LP_TAG_FRAME_DATA       = (LP_TAG_BASE * 3),
    /* Cursor metadata and texture data */
    LP_TAG_CURSOR_INFO      = (LP_TAG_BASE * 4),
    LP_TAG_CURSOR_DATA      = (LP_TAG_BASE * 5),
    /* 64-bit type */
    LP_TAG_MAX              = (1UL << 63)
};

enum lp_buffer_tags {
    LP_BUF_ANY,
    LP_BUF_SYS_INFO     = (1),
    LP_BUF_FRAME_INFO   = (1 << 1),
    LP_BUF_FRAME_DATA   = (1 << 2),
    LP_BUF_CURSOR_INFO  = (1 << 3),
    LP_BUF_CURSOR_DATA  = (1 << 4),
    LP_BUF_MAX          = (1 << 7)
};

enum lp_peer_state {
    LP_PS_INVALID,
    LP_PS_ACTIVE        = 1,
    LP_PS_STANDBY       = 2,
    LP_PS_MAX           = 255
};

typedef enum {
    LP_MSG_INVALID,
    LP_MSG_STAT      = 1,    // Peer state / keep alive
    LP_MSG_FRAME_REQ = 2,    // Frame request
    LP_MSG_KVMFR     = 3,    // KVMFR frame metadata update
    LP_MSG_METADATA  = 4,    // Metadata
    LP_MSG_CURSOR    = 5,    // Cursor update
    LP_MSG_MAX
} lp_msg_type;

#pragma pack(push, 1)

typedef struct {
    uint16_t    id;
} lp_msg_hdr;

typedef struct {
    lp_msg_hdr  hdr;
    uint8_t     peer_state;
    uint8_t     buffer_state;
    uint8_t     pause;
} lp_msg_state;

typedef struct {
    lp_msg_hdr  hdr;
    uint64_t    addr;
    uint64_t    rkey;
    uint64_t    size;
    uint64_t    offset;
    uint8_t     req_id;
} lp_msg_frame_req;

typedef struct {
    lp_msg_hdr  hdr;
    uint8_t     req_id;
    uint8_t     fmt;
    uint8_t     rot;
    uint32_t    fid;
    uint32_t    width;
    uint32_t    height;
    uint32_t    stride;
    uint32_t    pitch;
    uint32_t    flags;
} lp_msg_kvmfr;

typedef struct {
    lp_msg_hdr  hdr;
    uint16_t    lp_major;
    uint16_t    lp_minor;
    uint16_t    lp_patch;
    uint16_t    kvmfr_sockets;
    uint16_t    kvmfr_cores;
    uint16_t    kvmfr_threads;
    uint16_t    kvmfr_flags;
    uint16_t    kvmfr_os_id;
    uint16_t    kvmfr_fmt;
    uint32_t    width;
    uint32_t    height;
    uint32_t    stride;
    uint32_t    pitch;
    char        kvmfr_uuid[16];
    char        kvmfr_capture[32];
    char        cpu_model[64];
    char        os_name[32];
} lp_msg_metadata;

typedef struct {
    lp_msg_hdr  hdr;
    uint8_t     fmt;
    uint32_t    flags;
    uint16_t    pos_x;
    uint16_t    pos_y;
    uint8_t     hot_x;
    uint8_t     hot_y;
    uint32_t    width;
    uint32_t    height;
    uint32_t    pitch;
} lp_msg_cursor;

#pragma pack(pop)

#endif