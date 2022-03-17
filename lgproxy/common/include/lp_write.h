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
 * @return true on succes, false on error
 */
bool lpWriteFrame(PLPContext ctx, PTRFDisplay display);

#endif