#ifndef _LP_SINK_H
#define _LP_SINK_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

#include "lp_types.h"
#include "trf.h"
#include "trf_ncp.h"
#include "lp_log.h"
#include <signal.h>
#include <sys/mman.h>
#include "lp_trf.h"

/**
 * @brief Allocates Memory for lpAllocContext
 * 
 * @return PLPContext
 */
PLPContext lpAllocContext();


#endif