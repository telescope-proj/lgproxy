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