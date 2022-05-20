/*
    SPDX-License-Identifier: GPL-2.0-only

    Telescope Project  
    Looking Glass Proxy   
    Host side functions  
    
    Copyright (c) 2022 Telescope Project Developers

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
#ifndef _LP_RETRIEVE_H
#define _LP_RETRIEVE_H
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>
#include <math.h>

#include "lp_log.h"
#include "lgmp/client.h"
#include "lp_types.h"
#include "common/KVMFR.h"
#include "common/framebuffer.h"
#include "lp_convert.h"
#include "lp_utils.h"

/**
 * @param ctx               Context to use
 * @return 0 on success, negative error code on error
 */
int lpInitLgmpClient(PLPContext ctx);


/**
 * @brief Inittialize LGMP Client Session
 * @param ctx           Context to use
 * @return 0 on success, negative error code on error
 */
int lpClientInitSession(PLPContext ctx);

/**
 * @brief Get Frame from shared memory
 * 
 * @param ctx           Context to use
 * @param out           Frame retrieved from LookingGlass
 * @return 0 on success, negative error code on error
 */
int lpGetFrame(PLPContext ctx, KVMFRFrame **out, FrameBuffer **fb);

/**
 * @brief Get the Cursor object
 * 
 * @param ctx       Context to use
 * @param out       Cursor from Looking Glass Host if there is no change in cursor postion it out will be set to NULL
 * @return 0 on success, negative error code on failure
 */
int lpgetCursor(PLPContext ctx, KVMFRCursor **out, uint32_t *size, 
                uint32_t *flags);
#endif