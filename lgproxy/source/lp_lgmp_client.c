/*
    SPDX-License-Identifier: GPL-2.0-or-later

    Telescope Project  
    Looking Glass Proxy
    
    Copyright (c) 2022 - 2023 Telescope Project Developers

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

#include "module/kvmfr.h"
#include <sys/ioctl.h>
#include "lp_lgmp_client.h"

int lp_init_lgmp_client(const char * path, int * fd_out, void ** mem, 
                        size_t * size, PLGMPClient * cli)
{
    int ret = 0;
    int fd = open(path, O_RDWR, (mode_t) 0600);
    if (fd < 0)
    {
        lp__log_error("Unable to open shared file: %s", strerror(errno));
        ret = -errno;
        goto out;
    }
    if (strncmp(path, "/dev/kvmfr", 10) == 0)
    {
        *size = ioctl(fd, KVMFR_DMABUF_GETSIZE, 0);
    }
    else
    {
        struct stat st;
        if (stat(path, &st) != 0)
        {
            lp__log_error("Failed to stat shared memory file: %s", 
                          strerror(errno));
            return -errno;
        }
        *size = st.st_size;
    }
    lp__log_debug("Mapping %s, reported size: %lu", path, *size);
    void * ram = mmap(0, *size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 
                      0);
    if (ram == MAP_FAILED)
    {
        lp__log_error("Unable to map shared memory: %s", strerror(errno));
        ret = -errno;
        goto close_fd;
    }
    LGMP_STATUS status = lgmpClientInit(ram, *size, cli);
    if (status != LGMP_OK)
    {
        lp__log_error("LGMP client init failure: %s\n", lgmpStatusString(status));
        goto unmap;
    }
    *fd_out = fd;
    *mem = ram;
    return 0;

unmap:
    munmap(ram, *size);
    *size = 0;
    *mem = 0;
close_fd:
    close(fd);
out: 
    return ret;
}

int lp_enable_lgmp_client(PLGMPClient client, tcm_time * timeout, 
                          void ** data_out, size_t * data_size)
{
    int ret;
    struct timespec dl;
    ret = tcm_conv_time(timeout, &dl);
    if (ret < 0)
        return ret;

    uint32_t    udata_size;
    KVMFR       * udata;

    while (!tcm_check_deadline(&dl))
    {
        LGMP_STATUS stat = \
            lgmpClientSessionInit(client, &udata_size, (uint8_t **) &udata,
                                  NULL);
        switch (stat)
        {
            case LGMP_OK:
                *data_out = udata;
                *data_size = udata_size;
                return 0;
            case LGMP_ERR_INVALID_VERSION:
                lp__log_error("Incompatible LGMP version");
                lp__log_error("Please recompile LGProxy and/or Looking Glass "
                              "using matching LGMP versions");
                return -ENOTSUP;
            case LGMP_ERR_INVALID_SESSION:
            case LGMP_ERR_INVALID_MAGIC:
                tcm_sleep(timeout->interval);
                continue;
            default:
                lp__log_error("LGMP session init failed: %s", 
                              lgmpStatusString(stat));
                return -1;
        }
    }
    return -ETIMEDOUT;
}

int lp_subscribe_lgmp_client(PLGMPClient client, tcm_time * timeout,
                             PLGMPClientQueue * frame_q,
                             PLGMPClientQueue * ptr_q)
{
    int ret;
    struct timespec dl;
    ret = tcm_conv_time(timeout, &dl);
    if (ret < 0)
        return ret;

    LGMP_STATUS stat_frame, stat_ptr;
    stat_frame = stat_ptr = LGMP_ERR_INVALID_SESSION;
    while (!tcm_check_deadline(&dl))
    {
        if (stat_frame != LGMP_OK)
        {
            stat_frame = lgmpClientSubscribe(client, LGMP_Q_FRAME, frame_q);
        }
        if (stat_ptr != LGMP_OK)
        {
            stat_ptr = lgmpClientSubscribe(client, LGMP_Q_POINTER, ptr_q);
        }
        if (stat_frame == LGMP_OK && stat_ptr == LGMP_OK)
            return 0;
        tcm_sleep(timeout->interval);
    }
    if (stat_frame == LGMP_OK)
        lgmpClientUnsubscribe(frame_q);
    if (stat_ptr == LGMP_OK)
        lgmpClientUnsubscribe(ptr_q);
    return -ETIMEDOUT;
}

int lp_create_lgmp_client(const char * path, tcm_time * timeout, 
                          lp_lgmp_client_ctx ** ctx_out)
{
    int ret, fd;
    lp_lgmp_client_ctx * ctx = calloc(1, sizeof(*ctx));
    if (!ctx)
        return -ENOMEM;
    ret = lp_init_lgmp_client(path, &fd, &ctx->mem, &ctx->mem_size, &ctx->cli);
    if (ret < 0)
        goto cleanup_ctx;
    ret = lp_enable_lgmp_client(ctx->cli, timeout, &ctx->udata, &ctx->udata_size);
    if (ret < 0)
        goto cleanup_init;
    ret = lp_subscribe_lgmp_client(ctx->cli, timeout, &ctx->frame_q, &ctx->ptr_q);
    if (ret < 0)
        goto cleanup_init;
    *ctx_out = ctx;
    return ret;

cleanup_init:
    close(fd);
    lgmpClientFree(&ctx->cli);
cleanup_ctx:
    free(ctx);
    return ret;
}

int lp_resub_client(PLGMPClient client, PLGMPClientQueue * q, uint32_t q_id, 
                    tcm_time * timeout)
{
    int ret;
    struct timespec dl;
    ret = tcm_conv_time(timeout, &dl);
    if (ret < 0)
        return ret;

    LGMP_STATUS stat;
    do
    {
        stat = lgmpClientSubscribe(client, LGMP_Q_FRAME, q);
        if (stat != LGMP_OK)
        {
            tcm_sleep(timeout->interval);
            continue;
        }
    } while (!tcm_check_deadline(&dl));
    return -ETIMEDOUT;
}

int lp_request_lgmp_frame(PLGMPClient client, PLGMPClientQueue * frame_q, 
                          lp_lgmp_msg * out, tcm_time * timeout)
{
    int ret, q_reset = 0;
    struct timespec dl;
    ret = tcm_conv_time(timeout, &dl);
    if (ret < 0)
        return ret;

    LGMP_STATUS stat;
    LGMPMessage msg;
    do
    {
        stat = lgmpClientProcess(*frame_q, &msg);
        switch (stat)
        {
            case LGMP_OK:
                out->frame = (KVMFRFrame *) msg.mem;
                out->size  = msg.size;
                out->flags = msg.udata;
                return q_reset;
            case LGMP_ERR_QUEUE_EMPTY:
            case LGMP_ERR_INVALID_SESSION:
                tcm_sleep(timeout->interval);
                continue;
            case LGMP_ERR_QUEUE_TIMEOUT:
                lp__log_warn("Frame queue timed out");
                ret = lp_resub_client(client, frame_q, LGMP_Q_FRAME, timeout);
                if (ret < 0)
                    return ret;
                q_reset = 1;
                continue;
            default:
                tcm__log_error("LGMP frame request failed: %s", 
                               lgmpStatusString(stat));
                return -1;
        }
    } while (!tcm_check_deadline(&dl));
    return -EAGAIN;
}

/**
 * Request a cursor data update.
 *
 * @param client The LGMP client.
 * @param ptr_q Pointer to the LGMP client cursor data queue.
 * @param out Pointer to a KVMFRCursor struct where cursor metadata will be
 * returned.
 * @param size Output size of the shape data only, if any
 * @param flags KVMFRCursor flags
 * @param shape_out Buffer where shape data will be copied out to.
 * @param timeout Pointer to the timeout duration.
 *
 * @return 0 on success, negative error code on failure.
 */
int lp_request_lgmp_cursor(PLGMPClient client, PLGMPClientQueue * ptr_q,
                           lp_lgmp_msg * cursor,
                           void * shape_out, tcm_time * timeout)
{
    int ret, q_reset = 0;
    struct timespec dl;
    ret = tcm_conv_time(timeout, &dl);
    if (ret < 0)
        return ret;

    LGMP_STATUS stat;
    LGMPMessage msg;

    do
    {
        stat = lgmpClientProcess(*ptr_q, &msg);
        switch (stat)
        {
            case LGMP_OK:
                KVMFRCursor * tmp_cur = (KVMFRCursor *) msg.mem;
                size_t shape_size = (msg.udata & CURSOR_FLAG_SHAPE
                                    ? tmp_cur->height * tmp_cur->pitch : 0);
                if (msg.udata & CURSOR_FLAG_SHAPE)
                {
                    memcpy(shape_out, (tmp_cur + 1), shape_size - sizeof(*tmp_cur));
                }
                memcpy(cursor->cursor, tmp_cur, sizeof(*tmp_cur));
                lgmpClientMessageDone(*ptr_q);
                cursor->size = shape_size;
                cursor->flags = msg.udata;
                return q_reset;
            case LGMP_ERR_QUEUE_EMPTY:
            case LGMP_ERR_INVALID_SESSION:
                tcm_sleep(timeout->interval);
                return -EAGAIN;
            case LGMP_ERR_QUEUE_TIMEOUT:
                ret = lp_resub_client(client, ptr_q, LGMP_Q_POINTER, timeout);
                if (ret < 0)
                    return ret;
                q_reset = 1;
                continue;
            default:
                tcm__log_error("LGMP frame request failed: %s", 
                               lgmpStatusString(stat));
                return -1;
        }
    } while (!tcm_check_deadline(&dl));
    return -ETIMEDOUT;
}