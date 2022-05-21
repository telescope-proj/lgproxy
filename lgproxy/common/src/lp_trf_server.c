/*
    SPDX-License-Identifier: GPL-2.0-only

    Telescope Project  
    Looking Glass Proxy   
    LibTRF Host wrapper functions
    
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
#include "lp_trf_server.h"


int lpTrfServerInit(PLPContext ctx, char * host, char * port){
    int ret = 0;

    ctx->lp_host.server_ctx = trfAllocContext(); //Initialize Server Context
    if (!ctx->lp_host.server_ctx){
        lp__log_error("Unable to allocate server context");
        return -ENOMEM;
    }

    if((ret = trfNCServerInit(ctx->lp_host.server_ctx, host, port)) < 0)
    {
        lp__log_error("Unable to Initialize Server");
        goto destroy_server;
    }

    return 0;

destroy_server:
    trfDestroyContext(ctx->lp_host.server_ctx);
    return ret;
}

