/*
    SPDX-License-Identifier: GPL-2.0-or-later

    Telescope Project  
    Looking Glass Proxy   
    Type Definitions 
    
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
#include "lp_types.h"

PLPContext lpAllocContext(){
    PLPContext ctx = calloc(1, sizeof(* ctx));
    return ctx;
}

void lpDestroyContext(PLPContext ctx){
    if (!ctx)
        return;
        
    if (ctx->lp_client.lgmp_host)
    {
        lgmpHostFree(&ctx->lp_client.lgmp_host);
    }
    if (ctx->lp_host.lgmp_client)
    {
        lgmpClientFree(&ctx->lp_host.lgmp_client);
    }
    if (ctx->lp_client.client_ctx)
    {
        trfDestroyContext(ctx->lp_client.client_ctx);
    }
    if (ctx->lp_host.server_ctx)
    {
        trfDestroyContext(ctx->lp_host.server_ctx);
    }
    if (ctx->lp_host.client_ctx)
    {
        trfDestroyContext(ctx->lp_host.client_ctx);
    }
    if (ctx->ram)
    {
        munmap(ctx->ram, ctx->ram_size);
    }
    if (ctx->shmFile && ctx->opts.delete_exit)
    {
        close(ctx->shmFile);
    }
    free(ctx);
}