/*

    SPDX-License-Identifier: GPL-2.0-only

    Telescope Project 
    Looking Glass Proxy
    Custom Messaging Functions

    Copyright (c) 2022 Matthew John McMullin

    This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA. 

*/

#include "lp_msg.h"

int lpKeepAlive(PTRFContext ctx)
{
    if (!ctx)
    {
        return -EINVAL;
    }
    lp__log_trace("Keeping connection alive...");

    LpMsg__MessageWrapper mw = LP_MSG__MESSAGE_WRAPPER__INIT;
    LpMsg__KeepAlive ka = LP_MSG__KEEP_ALIVE__INIT;
    mw.wdata_case = LP_MSG__MESSAGE_WRAPPER__WDATA_KA;
    mw.ka = &ka;
    ka.info = 0;

    ssize_t dsize = trfMsgPackProtobuf((ProtobufCMessage *) &mw, 
                ctx->xfer.fabric->msg_mem.size,
                trfMemPtr(&ctx->xfer.fabric->msg_mem));
    if (dsize < 0)
    {
        lp__log_error("unable to encode data");
        return dsize;
    }

    lp__log_debug("Sending %ld bytes over fabric", dsize);

    return trfFabricSend(ctx, &ctx->xfer.fabric->msg_mem, 
                    trfMemPtr(&ctx->xfer.fabric->msg_mem),
                    dsize, ctx->xfer.fabric->peer_addr, 
                    ctx->opts);
}