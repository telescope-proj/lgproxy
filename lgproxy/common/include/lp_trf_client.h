/*
    SPDX-License-Identifier: GPL-2.0-only

    Telescope Project  
    Looking Glass Proxy   
    LibTRF Client wrapper functions
    
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
#ifndef _LP_TRF_CLIENT_H
#define _LP_TRF_CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

#include "lp_types.h"
#include "lp_log.h"
#include <signal.h>
#include <sys/mman.h>
#include "trf.h"
#include "trf_ncp.h"
#include "trf_ncp_client.h"


/**
 * @brief Initialize the Client connection to server
 * @param ctx           Context to use
 * @param host          Host IP address to connect to
 * @param port          Port to connect to on server
 * @return 0 on success, negative error code on error.
 */
int lpTrfClientInit(PLPContext ctx, char * host, char * port);



#endif