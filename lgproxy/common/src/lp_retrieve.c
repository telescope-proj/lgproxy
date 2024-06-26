/*
    SPDX-License-Identifier: GPL-2.0-or-later

    Telescope Project  
    Looking Glass Proxy   
    Host side functions  
    
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
#include "lp_retrieve.h"

int lpInitLgmpClient(PLPContext ctx)
{
    if (!ctx)
    {
        return -EINVAL;
    }
    int ret = 0;
    int fd = open(ctx->shm, O_RDWR, (mode_t) 0600);
    if (fd < 0)
    {
        lp__log_error("Unable to open shared file: %s", strerror(errno));
        ret = -errno;
        goto out;
    }
    ctx->ram = mmap(0, ctx->ram_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 
                    0);
    if (!ctx->ram)
    {
        lp__log_error("Unable to map shared memory: %s", strerror(errno));
        ret = -errno;
        goto close_fd;
    }
    LGMP_STATUS status;
    while ((status = lgmpClientInit(ctx->ram,ctx->ram_size, &ctx->lp_host.lgmp_client)) 
            != LGMP_OK)
    {
        lp__log_error("LGMP Client Init failure; %s\n", 
                    lgmpStatusString(status));
        goto unmap;
    }
    
    trfSleep(200); // Wait for host to update
    ctx->shmFile = fd;
    ctx->format_valid = false;

    return 0;

unmap:
    munmap(ctx->ram,ctx->ram_size);
close_fd:
    close(fd);
out: 
    return ret;
}

int lpClientInitSession(PLPContext ctx)
{
    if (!ctx)
    {
        return -EINVAL;
    }
    LGMP_STATUS status;
    uint32_t udataSize;
    KVMFR *udata;
    int waitCount = 0;
 
    for (int retry = 0; retry < 20; retry++)
    {
        status = lgmpClientSessionInit(ctx->lp_host.lgmp_client, &udataSize, 
                                       (uint8_t **) &udata, NULL);
        lp__log_trace("lgmpClientSessionInit: %s", lgmpStatusString(status));

        switch (status)
        {
            case LGMP_OK:
                ctx->state = LP_STATE_RUNNING;
                break;
            case LGMP_ERR_INVALID_VERSION:
                lp__log_debug("Incompatible LGMP Version"
                    "The host application is not compatible with this client"
                    "Please download and install the matching version");
                trfSleep(1000);
                if(retry == 19){
                    ctx->state = LP_STATE_STOP;
                    lp__log_debug("Incompatible LGMP Versions between" 
                            " client and host");
                    return -1;
                }
                continue;
            case LGMP_ERR_INVALID_SESSION:
            case LGMP_ERR_INVALID_MAGIC:
                if (waitCount++ == 0)
                {
                    ctx->state = LP_STATE_STOP;
                    lp__log_error("Host application does not seem to be running");
                }
                if (waitCount++ == 30)
                {
                    lp__log_debug(
                        "Host Application Not Running",
                        "It seems the host application is not running or your\n"
                        "virtual machine is still starting up\n"
                        "\n"
                        "If the the VM is running and booted please check the\n"
                        "host application log for errors. You can find the\n"
                        "log through the shortcut in your start menu\n"
                        "\n"
                        "Continuing to wait...");
                }
                trfSleep(1000);
                continue;
            default:
                ctx->state = LP_STATE_STOP;
                lp__log_error("lgmpClient Session Init failed: %s", 
                              lgmpStatusString(status));
                return -1;
        }

        if (ctx->state != LP_STATE_RUNNING)
        {
            lp__log_error("Unable to initialize session");
            return -1;
        }
        else
        {
            break;
        }
    }

    while (ctx->state == LP_STATE_RUNNING)
    {
        status = lgmpClientSubscribe(ctx->lp_host.lgmp_client, LGMP_Q_FRAME, 
                                    &ctx->lp_host.client_q);
        if (status == LGMP_OK)
        {
            lp__log_trace("lgmpClientSubscribed: %s", lgmpStatusString(status));
            break;
        }
        if (status == LGMP_ERR_NO_SUCH_QUEUE)
        {
            usleep(1000);
            continue;
        }
        lp__log_error("lgmpClientSubscribe: %s", lgmpStatusString(status));
        ctx->state = LP_STATE_STOP;
        break;
    }

    while (ctx->state == LP_STATE_RUNNING)
    {
        status = lgmpClientSubscribe(ctx->lp_host.lgmp_client, LGMP_Q_POINTER,
                &ctx->lp_host.pointer_q);
        {
            if (status == LGMP_OK)
            {
                break;
            }
            if (status == LGMP_ERR_NO_SUCH_QUEUE)
            {
                usleep(100);
                continue;
            }
            lp__log_error("Unable to subscribe to pointer queue: %s", 
                            lgmpStatusString(status));
            ctx->state = LP_STATE_STOP;
            break;
        }
    }
    return 0;
}

int lpGetFrame(PLPContext ctx, KVMFRFrame ** out, FrameBuffer ** fb)
{
    if (!ctx || !out){
        return - EINVAL;
    }

    LGMP_STATUS status;

    uint32_t          frameSerial = 0;
    uint32_t          formatVer   = 0;

    if (ctx->state != LP_STATE_RUNNING)
    {
        return 0;
    }
    
    LGMPMessage msg;
    while((status = lgmpClientProcess(ctx->lp_host.client_q, &msg)) != LGMP_OK)
    {
        if (status == LGMP_ERR_QUEUE_EMPTY)
        {
            struct timespec req =
            {
            .tv_sec  = 0,
            .tv_nsec = 10000
            };

            struct timespec rem;
            while(nanosleep(&req, &rem) < 0)
            {
            if (errno != -EINTR)
            {
                lp__log_error("nanosleep failed");
                break;
            }
            req = rem;
            }
            return -EAGAIN;
        }
        if (status == LGMP_ERR_INVALID_SESSION)
        {
            ctx->state = LP_STATE_RESTART;
        }
        if (status == LGMP_ERR_QUEUE_TIMEOUT)
        {
            status = lgmpClientSubscribe(ctx->lp_host.lgmp_client, 
                            LGMP_Q_FRAME, 
                            &ctx->lp_host.client_q);
            if (status != LGMP_OK)
            {
                ctx->state = LP_STATE_RESTART;
                return -1;
            }
            continue;
        }
        else
        {
            lp__log_error("lgmpClientProcess Failed: %s", 
            lgmpStatusString(status));
            ctx->state = LP_STATE_STOP;
            return -1;
        }
    }

    lp__log_trace("Frame offset: %lu", (uintptr_t) msg.mem - (uintptr_t) ctx->ram);
    KVMFRFrame * frame = (KVMFRFrame *)msg.mem;
    if (frame->frameSerial == frameSerial && ctx->format_valid)
    {
        lp__log_error("Repeated Frame");
        lgmpClientMessageDone(ctx->lp_host.client_q);
    }
    
    if (frame)
        lp__log_trace("Received frame size: %d x %d", 
                      frame->frameWidth, frame->frameHeight);
    
    frameSerial = frame->frameSerial;
    *out = frame;
    
    lp__log_trace("Frame Format: %d", lpLGToTrfFormat(frame->type));
    size_t ff = ((uint8_t *) frame - (uint8_t *) ctx->ram) + frame->offset;
    lp__log_trace("ff: %lu, fi->offset: %lu", ff, frame->offset);

    if (ctx->format_valid && frame->formatVer != formatVer)
    {
        if (frame->frameHeight != frame->screenHeight)
        {
            const int needed = ((frame->frameHeight * frame->pitch * 2) 
                            / 1048576.0f) + 10.0f;
            const int size = (int)powf(2.0f, ceilf(logf(needed) / logf(2.0f)));

            lp__log_warn("IVSHMEM too small, screen truncated");
            lp__log_warn("Recommend increase size to %d MiB", size);
        }
    }
    *fb = (FrameBuffer *)(((uint8_t*)frame) + frame->offset);
    lgmpClientMessageDone(ctx->lp_host.client_q);
    return 0;
}

int lpgetCursor(PLPContext ctx, KVMFRCursor **out, uint32_t *size, uint32_t *flags)
{
    LGMP_STATUS status;
    KVMFRCursor * cursor = NULL;
    LGMPMessage msg;
    int cursorSize = 0;

    while (ctx->state == LP_STATE_RUNNING)
    {
        status = lgmpClientProcess(ctx->lp_host.pointer_q, &msg);
        if (status == LGMP_OK)
        {
            break;
        }
        if (status == LGMP_ERR_QUEUE_EMPTY) // No change in cursor position
        {
            cursor = NULL;
            return 0;
        }
        if (status == LGMP_ERR_QUEUE_TIMEOUT)
        {
            status = lgmpClientSubscribe(ctx->lp_host.lgmp_client, 
                LGMP_Q_POINTER, &ctx->lp_host.pointer_q);
            if (status != LGMP_OK)
            {
                lp__log_error("Unable to resubscribe to pointer queue");
                return -1;
            }
            continue;
        }
        if (status == LGMP_ERR_INVALID_SESSION)
        {
            lp__log_error("Invalid Session");
            return -1;
        }
        else{
            lp__log_error("lgmpClientProcess Failed: %s", 
                lgmpStatusString(status));
            lgmpClientMessageDone(ctx->lp_host.pointer_q);
            return -1;
        }
    }
    KVMFRCursor *tmpCur = (KVMFRCursor *) msg.mem;
    const int sizeNeeded = sizeof(*tmpCur) + 
        (msg.udata & CURSOR_FLAG_SHAPE ? 
            tmpCur->height * tmpCur->pitch : 0);
    
    *flags = msg.udata; 

    if (cursor && sizeNeeded > cursorSize)
    {
        free(cursor);
        cursor = NULL;
    }
    if (!cursor)
    {
        cursor = malloc(sizeNeeded);
        if (!cursor)
        {
            lp__log_error("Unable to allocate memory for pointer data");
            return -ENOMEM;
        }
        cursorSize = sizeNeeded;
    }
    
    memcpy(cursor, msg.mem, sizeNeeded);

    lgmpClientMessageDone(ctx->lp_host.pointer_q);
    *out = cursor;
    *size = cursorSize;
    return 0;
}