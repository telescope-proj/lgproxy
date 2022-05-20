/*
    SPDX-License-Identifier: GPL-2.0-only

    Telescope Project  
    Looking Glass Proxy   
    Source Application
    
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