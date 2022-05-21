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
#include "lp_trf_client.h"


int lpTrfClientInit(PLPContext ctx, char * host, char * port){
    int ret = 0;
    ctx->lp_client.client_ctx = trfAllocContext(); //Initialize Client Context
    if (!ctx->lp_client.client_ctx){
        lp__log_error("Unable to allocate client context");
        return -ENOMEM;
    }

    if ((ret = trfNCClientInit(ctx->lp_client.client_ctx, host, port)) < 0)
    {
        lp__log_error("Unable to Initialize Client");
        goto destroy_client;
    }

    trf__log_debug("Client Initialized");
    
    return 0;
    
destroy_client:
    trfDestroyContext(ctx->lp_client.client_ctx);
    return ret;
}