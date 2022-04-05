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

LGMP_STATUS lpKeepLGMPSessionAlive(PLPContext ctx);

/**
 * @brief Initialize LGMP Host for receiving data from libtrf
 * 
 * @param ctx               PLPContext to use
 * @param display           Display data received
 * @return 0 on success, negative error code on failure
 */
int lpInitHost(PLPContext ctx, PTRFDisplay display);

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
#endif