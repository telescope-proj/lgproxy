#ifndef _LP_SOURCE_H
#define _LP_SOURCE_H

#include "trf_def.h"
#include "trf_ncp.h"
#include "trf.h"

#include "lp_trf_server.h"
#include "lp_retrieve.h"
#include "lp_log.h"
#include "lp_types.h"
#include "lp_convert.h"
#include "lp_msg.h"
#include "lp_utils.h"

#include <getopt.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include "common/framebuffer.h"
#include "lp_msg.pb-c.h"
#include <sys/stat.h>

/**
 * @brief This Function will handle all client side requests (e.g. Frames data, Cursor data)
 * 
 * @param ctx       Context containing the TRFContext for client connections
 */
int lpHandleClientReq(PLPContext ctx);

/**
 * @brief  Get the current cursor position and update the client side
 * @param  arg      PTRFContext containing connection details
 * 
 */
void * lpHandleCursorPos(void * arg);

#endif