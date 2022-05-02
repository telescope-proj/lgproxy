/*

    SPDX-License-Identifier: GPL-2.0-only

    Telescope Project 
    Looking Glass Proxy
    Utility Functions

    Copyright (c) 2022 Matthew John McMullin

    This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA. 

*/

#include "lp_utils.h"
#include "version.h"

int lpPollMsg(PLPContext ctx, TrfMsg__MessageWrapper ** msg)
{
    struct fi_cq_data_entry de;
    struct fi_cq_err_entry err;
    ssize_t ret;

    ret = trfFabricPollRecv(ctx->lp_client.client_ctx, &de, &err, 0, 0, NULL, 1);
    if (ret == 0 || ret == -EAGAIN)
    {
        return -EAGAIN;
    }
    if (ret < 0)
    {
        lp__log_error("Unable to poll CQ: %s", fi_strerror(-ret));
        return ret;
    }
    void *msgmem = trfMemPtr(&ctx->lp_client.client_ctx->xfer.fabric->msg_mem);
    ret = trfMsgUnpack(msg, 
                       trfMsgGetPackedLength(msgmem),
                       trfMsgGetPayload(msgmem));
    if (ret < 0)
    {
        lp__log_error("Unable to unpack: %s\n", 
            strerror(-ret));
        return ret;
    }
    return 0;
}

uint64_t lpParseMemString(char * data)
{
    char multiplier = data[strlen(data)-1];
    uint64_t b = atoi(data);
    if (multiplier >= '0' && multiplier <= '9')
    {
        return b;
    }
    else
    {
        switch (multiplier)
        {
            case 'K':
            case 'k':
                return b * 1024;
            case 'M':
            case 'm':
                return b * 1024 * 1024;
            case 'G':
            case 'g':
                return b * 1024 * 1024 * 1024;
            default:
                return 0;
        }
    }
}

bool lpShouldTruncate(PLPContext ctx)
{
    struct stat filestat;
    if (stat(ctx->shm, &filestat) < 0)
    {
        lp__log_error("Unable to get stats on shm file");
    }
    if (strncmp(ctx->shm, "/dev/kvmfr", sizeof("/dev/kvmfr")-1) == 0)
    {
        ctx->dma_buf = true;
        return false;
    }
    if (ctx->ram_size > filestat.st_size)
    {
        ctx->dma_buf = false;
        return true;
    }
    return false;
}


int lpSetDefaultOpts(PLPContext ctx)
{
    ctx->opts.poll_int = 1;
    ctx->shm = "/dev/shm/looking-glass";
    return 0;
}

int lpRoundUpFrameSize(int size)
{
    return (int) powf(2.0f, ceilf(logf(size) / logf(2.0f)));
}

int lpCalcFrameSizeNeeded(PTRFDisplay display)
{
    int needed = trfGetDisplayBytes(display) * 2 + \
        (sizeof(KVMFRCursor) + 1048576) * 2;
    return lpRoundUpFrameSize(needed);
}

int lpSendDisconnect(PTRFContext ctx)
{
    LpMsg__MessageWrapper mw = LP_MSG__MESSAGE_WRAPPER__INIT;
    LpMsg__Disconnect dc = LP_MSG__DISCONNECT__INIT;
    mw.disconnect = &dc;
    mw.wdata_case = LP_MSG__MESSAGE_WRAPPER__WDATA_DISCONNECT;
    mw.disconnect->info = 0;

    size_t dsize = trfMsgPackProtobuf((ProtobufCMessage *) &mw, 
                    ctx->xfer.fabric->msg_mem.size,
                    trfMemPtr(&ctx->xfer.fabric->msg_mem));
    if (dsize < 0)
    {
        lp__log_error("unable to encode data");
        return dsize;
    }

    ctx->disconnected = 1;

    return trfFabricSend(ctx, &ctx->xfer.fabric->msg_mem,
                        trfMemPtr(&ctx->xfer.fabric->msg_mem),
                        dsize, ctx->xfer.fabric->peer_addr,
                        ctx->opts);
}

int lpSendVersion(PTRFContext ctx)
{
    LpMsg__MessageWrapper mw = LP_MSG__MESSAGE_WRAPPER__INIT;
    LpMsg__BuildVersion version = LP_MSG__BUILD_VERSION__INIT;
    mw.build_version = &version;
    mw.wdata_case = LP_MSG__MESSAGE_WRAPPER__WDATA_BUILD_VERSION;
    mw.build_version->lg_version = (char *) LG_BUILD_VERSION;
    mw.build_version->lp_version = (char *) LP_BUILD_VERSION;

    size_t dsize = trfMsgPackProtobuf((ProtobufCMessage *) &mw,
                        ctx->xfer.fabric->msg_mem.size,
                        trfMemPtr(&ctx->xfer.fabric->msg_mem));
                        
    return trfFabricSend(ctx, &ctx->xfer.fabric->msg_mem,
                        trfMemPtr(&ctx->xfer.fabric->msg_mem),
                        dsize, ctx->xfer.fabric->peer_addr,
                        ctx->opts);
}