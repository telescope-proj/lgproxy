/*

    SPDX-License-Identifier: GPL-2.0-only

    Telescope Project 
    Looking Glass Proxy
    Client Functions

    Copyright (c) 2022 Matthew John McMullin

    This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA. 

*/

#ifndef _LP_WRITE_H
#define _LP_WRITE_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>
#include <math.h>

#include "lp_log.h"
#include "lgmp/host.h"
#include "lp_types.h"
#include "lp_convert.h"
#include "common/KVMFR.h"
#include "common/framebuffer.h"
#include "lp_utils.h"
#include "lp_msg.pb-c.h"

LGMP_STATUS lpKeepLGMPSessionAlive(PLPContext ctx, PTRFDisplay display);

/**
 * @brief Initialize LGMP Host for receiving data from libtrf
 * 
 * @param ctx               PLPContext to use
 * @param display           Display data received
 * @param initShm            Set to true if shm file needs to be initialized again
 * @return 0 on success, negative error code on failure
 */
int lpInitHost(PLPContext ctx, PTRFDisplay display, bool initShm);

/**
 * @brief       Signal that a frame is done writing to the LGMP client
 * 
 * @param ctx   Client context to use.
 * @param disp  Display data to write.
 * @return      int 
 */
int lpSignalFrameDone(PLPContext ctx, PTRFDisplay disp);

/**
 * @brief Write data to shared memory
 * 
 * @param ctx               PLPContext to use
 * @param display           Display data to write
 * @return 0 on success, negative error code on failure
 */
int lpRequestFrame(PLPContext ctx, PTRFDisplay disp);

/**
 * @brief Update host cursor position on the client
 * 
 * @param ctx        Context to use
 * @param cur        Cursor data
 * @return 0 on success, negative error code on failure
 */
int lpUpdateCursorPos(PLPContext ctx, KVMFRCursor *cur, uint32_t curSize, 
                uint32_t flags);

/**
 * @brief Post Update to LGMP Client
 * 
 * @param ctx       Context to use
 * @param flags     Cursor Flags
 * @param mem       PLGMPMemory where cursor data is stored
 * @return 0 on success, negative error code on failure
 */
int lpPostCursor(PLPContext ctx ,uint32_t flags, PLGMPMemory mem);

/**
 * @brief Open and Map SHM File
 * 
 * @param ctx       Context to use
 * @return 0 on success, negative error code on failure
 */
int lpInitShmFile(PLPContext ctx);

/**
 * @brief Shutdown all LGMP Host processes
 * 
 * @param ctx       Context to use
 */
void lpShutdown(PLPContext ctx);

/**
 * @brief Reinitialize Cursor Thread
 * 
 * @param ctx       Context to pass through to the thread
 * @return 0 on success, negative error code on failure,
 */
int lpReinitCursorThread(PLPContext ctx);

/**
 * @brief Handle Cursor Data
 * 
 * @param arg     PLPContext
 */
void * lpCursorThread(void * arg);
#endif