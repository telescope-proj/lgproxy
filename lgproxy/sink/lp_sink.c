/*
    SPDX-License-Identifier: GPL-2.0-only

    Telescope Project  
    Looking Glass Proxy   
    Sink Application
    
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

#include "tcm_fabric.h"
#include "tcm_errno.h"
#include "tcm_udp.h"
#include "tcmu.h"
#include "tcm.h"

#include "lp_lgmp_server.h"
#include "lp_version.h"
#include "lp_config.h"
#include "lp_sink.h"
#include "lp_msg.h"

#include "common/framebuffer.h"

volatile int8_t exit_flag = 0;

#define LP_RECV_Q_LEN   20
#define CID_DATA        0
#define CID_FRAME       1
#define SSAI            struct sockaddr_in *

void exitHandler(int dummy)
{
    if (exit_flag == 1)
    {
        lp__log_info("Force quitting...");
        exit(0);
    }
    exit_flag = 1;
}

enum { STATE_NONE, STATE_WAIT, STATE_REQ_SENT, STATE_READY, STATE_LGMP_POSTABLE,
       STATE_LGMP_POSTED, STATE_STANDBY, STATE_ACTIVE };

struct ctx {
    tcm_sock            beacon_sock;     /* UDP socket for initial startup */
    tcm_fabric          * fabrics[2];    /* Fabric channels (frame + data) */
    fi_addr_t           cur_peers[2];    /* Fabric addresses of the source (frame + data) */
    lp_lgmp_server_ctx  * lgmp_server;   /* Items required for LGMP interop */
    lp_msg_metadata     spec;            /* Cached system specs of LGMP host */
    void                * sock_buf;      /* Socket buffer for initial startup */
    tcm_time            timeout;         /* Global network I/O timeout */
    struct sockaddr_in  fabric_init[2];  /* Connection address for fabric */
    uint32_t            f_serial;        /* Local frame counter */

    /* Data flow states */

    uint8_t             lgmp_state;      /* State of the LGMP client */
    uint8_t             mouse_rx;        /* Mouse message (metadata) */
    uint8_t             mouse_shape_rx;  /* Mouse shape data only */
    uint8_t             frame_rx;        /* Frame message (metadata) */
    uint8_t             frame_data_rx;   /* Raw frame data only */
    uint8_t             inform_tx;       /* Status info */

    uint8_t             buf_states;     
    uint8_t             buf_last_sent;
    uint8_t             state_last_sent;
};

struct ctx ctx;
lp_config_opts opts;

void zero_ctx()
{
    ctx.beacon_sock = -1;
    ctx.fabrics[CID_DATA] = NULL;
    ctx.fabrics[CID_FRAME] = NULL;
    ctx.lgmp_server = NULL;
    ctx.sock_buf = NULL;
    ctx.timeout.delta = 1;
    ctx.timeout.interval = 0;
    ctx.timeout.ts.tv_sec = 0;
    ctx.timeout.ts.tv_nsec = 0;
}

uint32_t lp_cursor_to_kvmfr_cursor(lp_msg_cursor * in, KVMFRCursor * out)
{
    lp_msg_cursor tmp;
    lp_msg_cursor * src;
    if ((void *) in == (void *) out)
    {
        memcpy(&tmp, in, sizeof(*in));
        src = &tmp;
    }
    else
    {
        src = in;
    }
    KVMFRCursor * kc = (KVMFRCursor *) out;
    kc->x            = src->pos_x;
    kc->y            = src->pos_y;
    kc->type         = src->fmt;
    kc->hx           = src->hot_x;
    kc->hy           = src->hot_y;
    kc->width        = src->width;
    kc->height       = src->height;
    kc->pitch        = src->pitch;
    return src->flags;
}

uint32_t lp_frame_to_kvmfr_frame(lp_msg_kvmfr * in, KVMFRFrame * out)
{
    lp_msg_kvmfr tmp;
    lp_msg_kvmfr * src;
    if ((void *) in == (void *) out)
    {
        memcpy(&tmp, in, sizeof(*in));
        src = &tmp;
    }
    else
    {
        src = in;
    }
    out->formatVer        = 1;
    out->frameSerial      = src->fid;
    out->type             = src->fmt;
    out->frameWidth       = src->width;
    out->frameHeight      = src->height;
    out->screenWidth      = src->width;
    out->screenHeight     = src->height;
    out->rotation         = src->rot;
    out->stride           = src->stride;
    out->pitch            = src->pitch;
    out->offset           = lp_get_page_size() - FB_WP_SIZE;
    out->damageRectsCount = 0; /* No support for damage */
    out->flags            = 0; /* No support for setting cursor pos yet */
    return tmp.flags;
}

size_t get_frame_size(lp_msg_metadata * spec)
{
    switch (spec->kvmfr_fmt)
    {
        case FRAME_TYPE_BGRA:
        case FRAME_TYPE_RGBA:
        case FRAME_TYPE_RGBA10:
            return spec->width * spec->height * 4;
        case FRAME_TYPE_RGBA16F:
            return spec->width * spec->height * 8;
        default:
            return 0;
    }
}

int process_rx(tcm_time * post_timeout, tcm_time * wait_timeout,
               struct fi_cq_err_entry * err)
{
    lp_lgmp_server_ctx * ls = ctx.lgmp_server;
    int ret;

    /* Post message receives */

    if (ctx.mouse_rx == STATE_NONE)
    {
        ret = tcm_trecv_fabric(ctx.fabrics[CID_DATA], 
            lgmpHostMemPtr(ls->ptr_q_mem[ls->ptr_q_pos]), sizeof(lp_msg_cursor),
            ls->mr, ctx.cur_peers[CID_DATA], LP_TAG_CURSOR_INFO, LP_TAG_MASK,
            (uintptr_t *) ls->ptr_q_pos, post_timeout);
        if (ret < 0)
        {
            lp__log_error("Could not post mouse info receive: %s",
                            fi_strerror(-ret));
            return ret;
        }
        ctx.buf_states |= LP_BUF_CURSOR_INFO;
        ctx.mouse_rx = STATE_WAIT;
    }

    if (ctx.mouse_shape_rx == STATE_NONE)
    {
        ret = tcm_trecv_fabric(ctx.fabrics[CID_DATA], 
            lgmpHostMemPtr(ls->ptr_shape_mem[ls->ptr_shape_pos]) + sizeof(KVMFRCursor),
            MAX_POINTER_SIZE_ALIGN, ls->mr, ctx.cur_peers[CID_DATA],
            LP_TAG_CURSOR_DATA, LP_TAG_MASK, (uintptr_t *) ls->ptr_shape_pos,
            post_timeout);
        if (ret < 0)
        {
            lp__log_error("Could not post mouse info + texture receive: %s",
                            fi_strerror(-ret));
            return ret;
        }
        ctx.buf_states |= LP_BUF_CURSOR_DATA;
        ctx.mouse_shape_rx = STATE_WAIT;
    }

    if (ctx.frame_rx == STATE_NONE
        && lgmpHostQueuePending(ls->frame_q) < LGMP_Q_FRAME_LEN)
    {
        ret = tcm_trecv_fabric(ctx.fabrics[CID_DATA], 
            ((uint8_t *) lgmpHostMemPtr(ls->frame_q_mem[ls->frame_q_pos]) + 2048),
            sizeof(lp_msg_kvmfr), ls->mr, ctx.cur_peers[CID_DATA],
            LP_TAG_FRAME_INFO, LP_TAG_MASK, (uintptr_t *) ls->frame_q_pos,
            post_timeout);
        if (ret < 0)
        {
            lp__log_error("Could not post frame request: %s",
                            fi_strerror(-ret));
            return ret;
        }
        ctx.buf_states |= LP_BUF_FRAME_INFO;
        ctx.frame_rx = STATE_WAIT;
    }

    if (ctx.frame_data_rx == STATE_NONE
        && lgmpHostQueuePending(ls->frame_q) < LGMP_Q_FRAME_LEN)
    {
        ret = tcm_trecv_fabric(ctx.fabrics[CID_FRAME], 
            ((uint8_t *) lgmpHostMemPtr(ls->frame_q_mem[ls->frame_q_pos])) + lp_get_page_size(),
            get_frame_size(&ctx.spec), ls->mr, ctx.cur_peers[CID_FRAME],
            LP_TAG_FRAME_DATA, LP_TAG_MASK, (uintptr_t *) ls->frame_q_pos,
            post_timeout);
        if (ret < 0)
        {
            lp__log_error("Could not post frame request: %s",
                            fi_strerror(-ret));
            return ret;
        }
        ctx.buf_states |= LP_BUF_FRAME_DATA;
        ctx.frame_data_rx = STATE_WAIT;
    }

    if (lgmpHostQueuePending(ls->frame_q) == LGMP_Q_FRAME_LEN)
    {
        
    }

    /* Check receive states */

    if (ctx.mouse_rx == STATE_WAIT || ctx.mouse_shape_rx == STATE_WAIT
        || ctx.frame_rx == STATE_WAIT)
    {
        err->tag = 0;
        ret = tcm_wait_fabric(ctx.fabrics[CID_DATA]->rx_cq, wait_timeout, err);
        if (ret < 0 && ret != -EAGAIN && ret != -ETIMEDOUT)
        {
            lp__log_error("Wait failed: %s, tag: %p, len: %lu, overflow len: %lu, buffer: %p",
                            fi_strerror(-ret), err->tag, err->len, err->olen, err->buf);
            lp__log_error("Message header ID: %d", ((lp_msg_hdr *) err->buf)->id);
            if (ret != -FI_ETRUNC)
            {
                return ret;
            }
        }
        else if (ret > 0)
        {
            uint64_t tag = err->tag & ~LP_TAG_MASK;
            switch (tag)
            {
                case LP_TAG_CURSOR_INFO:
                    ctx.buf_states &= ~LP_BUF_CURSOR_INFO;
                    ctx.buf_last_sent &= ~LP_BUF_CURSOR_INFO;
                    ctx.mouse_rx = STATE_READY;
                    break;
                case LP_TAG_CURSOR_DATA:
                    ctx.buf_states &= ~LP_BUF_CURSOR_DATA;
                    ctx.buf_last_sent &= ~LP_BUF_CURSOR_DATA;
                    ctx.mouse_shape_rx = STATE_READY;
                    break;
                case LP_TAG_FRAME_INFO:
                    ctx.buf_states &= ~LP_BUF_FRAME_INFO;
                    ctx.buf_last_sent &= ~LP_BUF_FRAME_DATA;
                    ctx.frame_rx = STATE_READY;
                    break;
                case 0:
                    lp__log_error("No tag provided! Msg len: %lu", err->len);
                    lp__log_error("Message header ID: %d", ((lp_msg_hdr *) err->buf)->id);
                    return ret;
                default:
                    lp__log_error("Invalid message tag on data channel: "
                                    "%p (%p)", err->tag, tag);
                    return ret;
            }
        }
    }

    if (ctx.frame_data_rx == STATE_WAIT)
    {
        ret = tcm_wait_fabric(ctx.fabrics[CID_FRAME]->rx_cq, wait_timeout, err);
        if (ret < 0 && ret != -EAGAIN && ret != -ETIMEDOUT)
        {
            lp__log_error("Wait failed: %s, tag: %p, len: %lu",
                            fi_strerror(-ret), err->tag, err->len);
            return ret;
        }
        else if (ret > 0)
        {
            uint64_t tag = err->tag & ~LP_TAG_MASK;
            switch (tag)
            {
                case LP_TAG_FRAME_DATA:
                    ctx.buf_states &= ~LP_BUF_FRAME_DATA;
                    ctx.frame_data_rx = STATE_READY;
                    break;
                default:
                    lp__log_error("Invalid message tag on frame channel: "
                                    "%p (%p)", err->tag, tag);
                    return ret;
            }
        }
    }

    return 0;
}

int process_rcvd_msgs()
{
    lp_lgmp_server_ctx * ls = ctx.lgmp_server;
    int procd = 0;

    if (ctx.mouse_rx == STATE_READY)
    {
        void * buf = lgmpHostMemPtr(ls->ptr_q_mem[ls->ptr_q_pos]);
        if (!buf)
            return -ENOBUFS;
        uint32_t udata = lp_cursor_to_kvmfr_cursor(buf, buf);
        ls->ptr_udata[ls->ptr_q_pos] = udata;
        ctx.mouse_rx = STATE_LGMP_POSTABLE;
        procd++;
    }

    if (ctx.mouse_rx == STATE_READY)
    {
        ctx.mouse_shape_rx = STATE_LGMP_POSTABLE;
    }

    if (ctx.frame_rx == STATE_READY)
    {
        void * buf = lgmpHostMemPtr(ls->frame_q_mem[ls->frame_q_pos]);
        if (!buf)
            return -ENOBUFS;
        uint32_t udata = lp_frame_to_kvmfr_frame(
            (lp_msg_kvmfr *) ((uint8_t *) buf + 2048), (KVMFRFrame *) buf);
        ls->frame_udata[ls->frame_q_pos] = udata;
        ctx.frame_rx = STATE_LGMP_POSTABLE;
        procd++;
    }

    if (ctx.frame_data_rx == STATE_READY)
    {
        ctx.frame_data_rx = STATE_LGMP_POSTABLE;
    }

    return procd;
}

int process_lgmp()
{
    lp_lgmp_server_ctx * ls = ctx.lgmp_server;
    PLGMPMemory payload;
    uint32_t udata;
    LGMP_STATUS status;
    int procd = 0;

    /* Post messages with valid buffers */

    if (ctx.mouse_rx == STATE_LGMP_POSTABLE)
    {
        payload = ls->ptr_q_mem[ls->ptr_q_pos];
        udata = ls->ptr_udata[ls->ptr_q_pos];
        if (udata & ~CURSOR_FLAG_SHAPE)
        {
            lgmpHostQueueNewSubs(ls->ptr_q);
            if (lgmpHostQueuePending(ls->ptr_q) < LGMP_Q_POINTER_LEN)
            {
                status = lgmpHostQueuePost(ls->ptr_q, udata, payload);
                if (status != LGMP_OK)
                {
                    lp__log_error("Failed to post cursor to LGMP queue: %s",
                                lgmpStatusString(status));
                    return -EBUSY;
                }
                if (++ls->ptr_q_pos == LGMP_Q_POINTER_LEN)
                    ls->ptr_q_pos = 0;
                ctx.mouse_rx = STATE_NONE;
            }
            procd++;
        }
    }

    if (ctx.mouse_shape_rx == STATE_LGMP_POSTABLE
        && ls->ptr_udata[ls->ptr_q_pos] & CURSOR_FLAG_SHAPE
        && ctx.mouse_shape_rx == STATE_LGMP_POSTABLE)
    {
        payload = ls->ptr_shape_mem[ls->ptr_shape_pos];
        udata = ls->ptr_udata[ls->ptr_q_pos];

        lgmpHostQueueNewSubs(ls->ptr_q);
        if (lgmpHostQueuePending(ls->ptr_q) < LGMP_Q_POINTER_LEN)
        {
            status = lgmpHostQueuePost(ls->ptr_q, udata, payload);
            if (status != LGMP_OK)
            {
                lp__log_error("Failed to post cursor + shape to LGMP queue: %s",
                              lgmpStatusString(status));
                return -EBUSY;
            }
            if (++ls->ptr_shape_pos == POINTER_SHAPE_BUFFERS)
                ls->ptr_shape_pos = 0;
            if (++ls->ptr_q_pos == LGMP_Q_POINTER_LEN)
                ls->ptr_q_pos = 0;
            ctx.mouse_shape_rx = STATE_NONE;
            ctx.mouse_rx = STATE_NONE;
        }
        procd++;
    }

    if (ctx.frame_rx == STATE_LGMP_POSTABLE)
    {
        payload = ls->frame_q_mem[ls->frame_q_pos];
        udata = ls->frame_udata[ls->frame_q_pos];
        KVMFRFrame * fi = (void *) lgmpHostMemPtr(payload);
        FrameBuffer * fb = (void *) (((uint8_t *) fi) + fi->offset);
            framebuffer_set_write_ptr(fb, 0);
        lgmpHostQueueNewSubs(ls->frame_q);
        if (lgmpHostQueuePending(ls->frame_q) < LGMP_Q_FRAME_LEN)
        {
            status = lgmpHostQueuePost(ls->frame_q, udata, payload);
            if (status != LGMP_OK)
            {
                lp__log_error("Failed to post frame to LGMP queue: %s",
                              lgmpStatusString(status));
                return -EBUSY;
            }
            ctx.frame_rx = STATE_LGMP_POSTED;
        }
        procd++;
    }
    
    if (ctx.frame_rx == STATE_LGMP_POSTED
        && ctx.frame_data_rx == STATE_LGMP_POSTABLE)
    {
        lp__log_trace("Updating WP pos %lu", ls->frame_q_pos);
        payload = ls->frame_q_mem[ls->frame_q_pos];
        KVMFRFrame * fi = lgmpHostMemPtr(payload);
        if (fi->rotation != 0)
            lp__log_trace("rot: %lu, typ: %lu", fi->rotation, fi->type);
        FrameBuffer * fb = (void *) (((uint8_t *) fi) + fi->offset);
        framebuffer_set_write_ptr(fb, fi->pitch * fi->frameHeight);
        if (++ls->frame_q_pos == LGMP_Q_FRAME_LEN)
            ls->frame_q_pos = 0;
        ctx.frame_data_rx = STATE_NONE;
        ctx.frame_rx = STATE_NONE;
    }

    return procd;
}

void cleanup_ctx()
{
    if (ctx.beacon_sock)
        close(ctx.beacon_sock);
    if (ctx.sock_buf)
        free(ctx.sock_buf);
    if (ctx.lgmp_server)
    {
        for (int i = 0; i < LGMP_Q_FRAME_LEN; i++)
        {
            lgmpHostMemFree(&ctx.lgmp_server->frame_q_mem[i]);
        }
        for (int i = 0; i < RX_Q_LEN; i++)
        {
            lgmpHostMemFree(&ctx.lgmp_server->ptr_q_mem[i]);
        }
        for (int i = 0; i < POINTER_SHAPE_BUFFERS; i++)
        {
            lgmpHostMemFree(&ctx.lgmp_server->ptr_shape_mem[i]);
        }
        lgmpHostFree(&ctx.lgmp_server->host);
        if (ctx.lgmp_server->mem)
            munmap(ctx.lgmp_server->mem, ctx.lgmp_server->mem_size);
        close(ctx.lgmp_server->dma_fd);
        close(ctx.lgmp_server->fd);
    }
    tcm_destroy_fabric(ctx.fabrics[CID_DATA], 1);
    tcm_destroy_fabric(ctx.fabrics[CID_FRAME], 1);
}

int init_host()
{
    ssize_t ret;
    struct fi_cq_err_entry err;
    lp_msg_metadata * mtd = ctx.fabrics[CID_DATA]->mr_info.ptr;
    ret = tcm_trecv_fabric(ctx.fabrics[CID_DATA], mtd, sizeof(*mtd),
                           ctx.fabrics[CID_DATA]->mr, ctx.cur_peers[CID_DATA],
                           LP_TAG_SYS_INFO, LP_TAG_MASK, 0, &ctx.timeout);
    if (ret < 0)
    {
        lp__log_error("Could not receive metadata from server: %s",
                      fi_strerror(-ret));
        return ret;
    }
    
    ret = tcm_wait_fabric(ctx.fabrics[CID_DATA]->rx_cq, &ctx.timeout, &err);
    if (ret < 0)
    {
        lp__log_error("Could not receive metadata from server: %s",
                fi_strerror(-ret));
        return ret;
    }
    if (err.len != sizeof(*mtd))
    {
        lp__log_error("Invalid message size: expected %lu, got %lu", 
                      sizeof(*mtd), err.len);
        return -EBADMSG;
    }

    /* Verify LGProxy version */
    if (mtd->lp_major != LP_VERSION_MAJOR
        || mtd->lp_minor != LP_VERSION_MINOR
        || mtd->lp_patch != LP_VERSION_PATCH)
    {
        lp__log_error("LGProxy version mismatch. Server: %d.%d.%d, Client: %d.%d.%d",
            mtd->lp_major, mtd->lp_minor, mtd->lp_patch,
            LP_VERSION_MAJOR, LP_VERSION_MINOR, LP_VERSION_PATCH);
        return -ENOTSUP;
    }

    /* Copy data from message into KVMFR structures */
    KVMFRRecord_OSInfo * os_info = calloc(1, 
        sizeof(*os_info) + 32);
    if (!os_info)
        return -ENOMEM;
    KVMFRRecord_VMInfo * vm_info = calloc(1, 
        sizeof(*vm_info) + 64);
    if (!vm_info)
        return -ENOMEM;
    /* Null-terminate in case server sent garbage strings */
    mtd->os_name[sizeof(mtd->os_name) - 1] = '\0';
    mtd->cpu_model[sizeof(mtd->cpu_model) - 1] = '\0';
    mtd->kvmfr_capture[sizeof(mtd->kvmfr_capture) - 1] = '\0';
    os_info->os = mtd->kvmfr_os_id;
    memcpy(os_info->name, mtd->os_name, 32);
    memcpy(vm_info->model, mtd->cpu_model, 64);
    snprintf(vm_info->capture, 31, "LGProxy/%.22s", mtd->kvmfr_capture);
    memcpy(vm_info->uuid, mtd->kvmfr_uuid, sizeof(mtd->kvmfr_uuid));
    vm_info->sockets = mtd->kvmfr_sockets;
    vm_info->cores = mtd->kvmfr_cores;
    vm_info->cpus = mtd->kvmfr_threads;

    /* Take a copy of the metadata */
    memcpy(&ctx.spec, mtd, sizeof(*mtd));
    
    /* Now we have all the data we need, init LGMP host */
    ret = lp_init_lgmp_host(ctx.lgmp_server, vm_info, os_info, 
                            mtd->kvmfr_flags, mtd->height * mtd->stride);
    if (ret < 0)
    {
        lp__log_error("Could not initialize LGMP host: %s", strerror(ret));
        return ret;
    }

    /*  Register the LGMP buffer for RDMA operations. Not sure how well this
        handles DMABUF-backed MRs yet. */
    ret = fi_mr_reg(ctx.fabrics[CID_FRAME]->domain, ctx.lgmp_server->mem,
                    ctx.lgmp_server->mem_size, FI_READ | FI_WRITE | FI_REMOTE_WRITE,
                    0, 0, 0, &ctx.lgmp_server->mr, NULL);
    if (ret < 0)
    {
        lp__log_error("Could not register memory region for RDMA: %s",
                      fi_strerror(-ret));
        return ret;
    }

    free(os_info);
    free(vm_info);
    lp__log_debug("Host initialized");
    return 0;
}

void inform_state(int16_t override_buf)
{
    uint8_t cur_state = (ctx.lgmp_state == STATE_ACTIVE);
    if ((ctx.inform_tx == STATE_NONE 
        && ctx.state_last_sent != cur_state) || override_buf > 0)
    {
        tcm_fabric * d_fab = ctx.fabrics[CID_DATA];
        fi_addr_t d_peer = ctx.cur_peers[CID_DATA];
        lp_msg_state * stat = d_fab->mr_info.ptr;
        ssize_t ret;
        stat->peer_state = cur_state;
        if (override_buf < 0)
            stat->buffer_state = ctx.buf_states;
        else
            stat->buffer_state = (uint8_t) override_buf;
        ret = tcm_tsend_fabric(d_fab, stat, sizeof(*stat), d_fab->mr, d_peer,
                                LP_TAG_STAT, 0, &ctx.timeout);
        if (ret == 0)
        {
            ctx.state_last_sent = cur_state;
            ctx.inform_tx = STATE_WAIT;
        }
    }
}

int activate_proxy()
{
    /* State and variable storage stuff */
    ssize_t ret;
    struct fi_cq_err_entry err;
    lp_lgmp_server_ctx * ls = ctx.lgmp_server;
    
    /* Wait for metadata message from server before starting */
    ret = init_host();
    if (ret < 0)
    {
        lp__log_error("Host init error: %s", strerror(ret));
        return ret;
    }
    
    /* Wait for an LGMP host to connect */
    int retries = 0;
    
    tcm_time once = {
        .ts.tv_sec = 0,
        .ts.tv_nsec = 0,
        .interval = 0,
        .delta = 1
    };
    ls->frame_q_pos = 0;
    ls->ptr_q_pos = 0;
    ctx.f_serial = 0;

    // lp_dbuf * base_ptr = ctx.fabrics[CID_DATA]->mr_info.ptr;
    // lp_dbuf * send_buf = base_ptr;

    ctx.mouse_rx       = STATE_NONE;
    ctx.mouse_shape_rx = STATE_NONE;
    ctx.frame_rx       = STATE_NONE;
    ctx.frame_data_rx  = STATE_NONE;
    ctx.lgmp_state     = STATE_ACTIVE;
    ctx.lgmp_server->ptr_q_pos   = 0;
    ctx.lgmp_server->frame_q_pos = 0;
    ctx.buf_last_sent  = 0;
    ctx.buf_states     = 0;

    while (1)
    {
        if (ctx.lgmp_state == STATE_ACTIVE)
            tcm_fsleep(opts.poll_interval);
        else
            tcm_sleep(200);

        LGMP_STATUS status;
        status = lgmpHostProcess(ls->host);
        if (status != LGMP_OK && status != LGMP_ERR_QUEUE_EMPTY)
        {
            lp__log_error("lgmpHostProcess failed: %s", lgmpStatusString(status));
            return -1;
        }

        /* Inform the other side if the LGMP session is active */

        inform_state(-1);
        if (ctx.inform_tx == STATE_WAIT)
        {
            ret = tcm_wait_fabric(ctx.fabrics[CID_DATA]->tx_cq, &once, &err);
            if (ret < 0 && ret != -EAGAIN && ret != -ETIMEDOUT)
            {
                lp__log_error("Fabric error: %s", fi_strerror(-ret));
                return ret;
            }
            if (ret > 0)
                ctx.inform_tx = STATE_NONE;
        }

        /* Wait for an LGMP client */
        if (!lgmpHostQueueHasSubs(ls->frame_q) &&
            !lgmpHostQueueHasSubs(ls->ptr_q))
        {
            retries++;
            if (retries > 200)
            {
                ctx.lgmp_state = STATE_STANDBY;
                if (retries % 10 == 0)
                {
                    ret = tcm_send_dummy_message(ctx.fabrics[CID_DATA], 
                        ctx.cur_peers[CID_DATA], &ctx.timeout);
                    if (ret < 0)
                    {
                        lp__log_error("Data channel error: %s", fi_strerror(-ret));
                        return ret;
                    }
                    ret = tcm_send_dummy_message(ctx.fabrics[CID_FRAME], 
                        ctx.cur_peers[CID_FRAME], &ctx.timeout);
                    if (ret < 0)
                    {
                        lp__log_error("Data channel error: %s", fi_strerror(-ret));
                        return ret;
                    }
                    lp__log_trace("Keeping connection alive");
                }
            }
        }
        else
        {
            retries = 0;
            ctx.lgmp_state = STATE_ACTIVE;
        }

        /* Only perform work if there is an active LGMP client */
        if (ctx.lgmp_state == STATE_STANDBY)
            continue;

        /* Queue and wait for receives */
        ret = process_rx(&ctx.timeout, &once, &err);
        if (ret < 0)
            return ret;

        /* Process received messages (if any) */
        ret = process_rcvd_msgs();
        if (ret < 0)
            return ret;

        /* Push processed messages to LGMP queue (if any) */
        ret = process_lgmp();
        if (ret < 0)
            return ret;


        /* todo: Relay cursor align messages */

        /* Check if the user wants to quit */

        if (exit_flag)
        {
            lp__log_info("Quit request received, shutting down...");
            return 0;
        }
    }
}

int main(int argc, char ** argv)
{
    lp_set_log_level(); 
    signal(SIGINT, exitHandler);
    ssize_t ret;
    
    ret = lp_load_cmdline(argc, argv, 0, &opts);
    if (ret < 0)
    {
        lp__log_fatal("Failed to load command line options: %s", strerror(-ret));
        lp_print_usage(0);
        return -ret;
    }

    /* Check the fabric is supported and set the transport ID */
    int tid = lp_prov_name_to_tid(opts.transport);
    if (tid < 0)
    {
        lp__log_fatal("Transport provider %s unsupported: %s", opts.transport,
                      strerror(-tid));
        return -tid;
    }

    lp_set_log_level();
    zero_ctx();

    /* Create the SHM region (without LGMP) */
    ctx.lgmp_server = calloc(1, sizeof(*ctx.lgmp_server));
    if (!ctx.lgmp_server)
        return ENOMEM;
    ctx.lgmp_server->fd     = -1;
    ctx.lgmp_server->dma_fd = -1;
    ctx.lgmp_server->mem_size = opts.mem_size;
    ret = lp_init_mem_server(opts.mem_path, &ctx.lgmp_server->fd,
                             &ctx.lgmp_server->dma_fd, &ctx.lgmp_server->mem,
                             &ctx.lgmp_server->mem_size);
    if (ret < 0)
    {
        lp__log_fatal("SHM file open failed: %s", strerror(-ret));
        return ENOMEM;
    }

    /* Create UDP socket */

    ret = tcm_setup_udp(opts.bind_addr, TCM_SOCK_MODE_SYNC, &ctx.beacon_sock);
    if (ret < 0)
    {
        lp__log_fatal("Beacon socket creation failed: %s", strerror(-ret));
        return -ret;
    }

    /*  Create fabric channels */

    ctx.fabrics[CID_DATA]  = calloc(1, sizeof(tcm_fabric));
    ctx.fabrics[CID_FRAME] = calloc(1, sizeof(tcm_fabric));
    if (!ctx.fabrics[CID_DATA] || !ctx.fabrics[CID_FRAME])
    {
        lp__log_fatal("Fabric structure alloc failed: insufficient memory");
        cleanup_ctx();
        return ENOMEM;
    }
    ret = tcmu_create_endpoint(opts.data_addr, opts.transport, opts.fabric_ver,
                               ctx.fabrics[CID_DATA], 
                               MAX_POINTER_SIZE_ALIGN * 2);
    if (ret < 0)
    {
        lp__log_fatal("Endpoint creation failed: %s", strerror(-ret));
        cleanup_ctx();
        return -ret;
    }
    ret = tcmu_create_endpoint(opts.frame_addr, opts.transport, opts.fabric_ver,
                              ctx.fabrics[CID_FRAME], lp_get_page_size());
    if (ret < 0)
    {
        lp__log_fatal("Endpoint creation failed: %s", strerror(-ret));
        cleanup_ctx();
        return -ret;
    }

    ctx.fabrics[CID_DATA]->transport_id  = tid;
    ctx.fabrics[CID_FRAME]->transport_id = tid;

    ctx.sock_buf = calloc(1, 1024);
    if (!ctx.sock_buf)
    {
        lp__log_fatal("Not enough memory");
        cleanup_ctx();
        return ENOMEM;
    }

    ret = tcm_set_timeout_udp(ctx.beacon_sock, 3000, 3000);
    if (ret < 0)
    {
        lp__log_fatal("Could not set socket timeout");
        cleanup_ctx();
        return -ret;
    }

    ctx.timeout.delta       = 1;
    ctx.timeout.interval    = opts.poll_interval;
    ctx.timeout.ts.tv_sec   = 3;
    ctx.timeout.ts.tv_nsec  = 0;
    ssize_t msize = 0;

    /*  Begin data exchange with server:
        ping -> channel 0 -> connect 0 -> channel 1 -> connect 1 -> active */
    tcm_msg_client_ping * ping = ctx.sock_buf;
    tcm_msg_server_status * stat = (tcm_msg_server_status *)(ping + 1);
    ping->common.id = TCM_MSG_CLIENT_PING;
    ping->pad = 0;
    tcm_msg_init(ping, 100);
    msize = tcm_exch_udp(ctx.beacon_sock, ping, sizeof(*ping), 
                       stat, sizeof(*stat), opts.connect_addr, &ctx.timeout);
    if (msize < 0)
    {
        lp__log_error("Data exchange failed: %s", strerror(-msize));
        return -msize;
    }
    ret = tcm_msg_verify((tcm_msg_header *) stat, msize);
    if (ret < 0)
    {
        lp__log_error("Invalid server response of size %lu: %s", msize, 
                      strerror(-ret));
        return -ret;
    }
    if (stat->version.major != TCM_VERSION_MAJOR
        || stat->version.minor != TCM_VERSION_MINOR
        || stat->version.patch != TCM_VERSION_PATCH)
    {
        lp__log_error("TCM API version mismatch: Server %d.%d.%d, Client %d.%d.%d",
                      stat->version.major, stat->version.minor, stat->version.patch,
                      TCM_VERSION_MAJOR, TCM_VERSION_MINOR, TCM_VERSION_PATCH);
        lp__log_fatal("Install matching LGProxy software on both sides and try again");
        return ENOTSUP;
    }

    /* Request and add server address of channel 0 (CID_DATA) and 1 (CID_FRAME) */
    for (int i = 0; i < 2; i++)
    {
        tcm_msg_metadata_req * req = ctx.sock_buf;
        tcm_msg_metadata_resp * resp = (tcm_msg_metadata_resp *)(req + 1);
        req->common.id = TCM_MSG_METADATA_REQ;
        tcm_msg_init(req, 200 + i);
        req->cid = i;
        msize = tcm_exch_udp(ctx.beacon_sock, req, sizeof(*req), 
                        stat, 256, opts.connect_addr, &ctx.timeout);
        if (msize < 0)
        {
            lp__log_error("Data exchange failed: %s", strerror(-msize));
            return -msize;
        }
        ret = tcm_msg_verify((tcm_msg_header *) stat, msize);
        if (ret < 0)
        {
            lp__log_error("Invalid server response of size %lu: %s", msize, 
                        strerror(-ret));
            return -ret;
        }
        if (resp->addr_fmt != TCM_AF_INET || resp->addr_len != sizeof(tcm_addr_inet))
        {
            lp__log_error("Server sent invalid address data");
            return EBADMSG;
        }
        ctx.fabric_init[i].sin_family = AF_INET;
        ctx.fabric_init[i].sin_addr.s_addr = \
            ((tcm_addr_inet *) resp->addr)->addr;
        ctx.fabric_init[i].sin_port = ((tcm_addr_inet *) resp->addr)->port;
        ret = tcmu_add_peer(ctx.fabrics[i], (struct sockaddr *) &ctx.fabric_init[i],
                            &ctx.cur_peers[i]);
        if (ret < 0)
        {
            lp__log_error("Could not add peer address to AV: %s", fi_strerror(-ret));
            return -ret;
        }
        lp__log_debug("Peer added on CID %lu: %lu", i, ctx.cur_peers[i]);
    }

    /* Send connection requests for both channels */
    for (int i = 0; i < 2; i++)
    {
        tcm_msg_conn_req * req = ctx.sock_buf;
        tcm_msg_server_status * resp = (tcm_msg_server_status *)
            ((uint8_t *)(req + 1) + sizeof(tcm_addr_inet));
        tcm_addr_inet * net = (void *) req->addr;
        req->common.id = TCM_MSG_CONN_REQ;
        tcm_msg_init(req, 300 + i);
        req->cid = i;
        switch (req->cid)
        {
            case CID_DATA:
                net->addr = ((SSAI) opts.data_addr)->sin_addr.s_addr;
                net->port = ((SSAI) opts.data_addr)->sin_port;
                req->tid = ctx.fabrics[CID_DATA]->transport_id;
                break;
            case CID_FRAME:
                net->addr = ((SSAI) opts.frame_addr)->sin_addr.s_addr;
                net->port = ((SSAI) opts.frame_addr)->sin_port;
                req->tid = ctx.fabrics[CID_FRAME]->transport_id;
                break;
        }
        req->addr_len = sizeof(tcm_addr_inet);
        req->addr_fmt = TCM_AF_INET;
        req->fabric_major = FI_MAJOR(opts.fabric_ver);
        req->fabric_minor = FI_MINOR(opts.fabric_ver);
        lp__log_debug("Addr data: %d %d %d %d", ntohl(net->addr), ntohs(net->port), 
        ntohl(((struct sockaddr_in *) opts.bind_addr)->sin_addr.s_addr), 
        ntohs(((struct sockaddr_in *) opts.bind_addr)->sin_port));
        msize = tcm_exch_udp(ctx.beacon_sock, req, sizeof(*req) + req->addr_len, 
                        resp, 256, opts.connect_addr, &ctx.timeout);
        if (msize < 0)
        {
            lp__log_error("Data exchange failed: %s", strerror(-msize));
            return -msize;
        }
        ret = tcm_msg_verify((tcm_msg_header *) stat, msize);
        if (ret < 0)
        {
            lp__log_error("Invalid server response of size %lu: %s", msize, 
                        strerror(-ret));
            return -ret;
        }
        if (resp->retcode != 0)
        {
            lp__log_error("Server rejected connection request: %s",
                          strerror(tcm_err_to_sys(resp->retcode)));
            return EBADMSG;
        }
        else
            lp__log_debug("Server acknowledged connection request on CID %lu", i);
    }

    /* Exchange small messages to verify the link works */
    struct fi_cq_err_entry err;
    ret = tcm_exch_fabric(ctx.fabrics[CID_DATA],
        ctx.fabrics[CID_DATA]->mr_info.ptr, 1, ctx.fabrics[CID_DATA]->mr,
        ((uint8_t *) ctx.fabrics[CID_DATA]->mr_info.ptr) + 1, 1,
        ctx.fabrics[CID_DATA]->mr, ctx.cur_peers[CID_DATA], 
        &ctx.timeout, &err);
    if (ret < 0)
    {
        lp__log_error("Timed out waiting for message on data channel");
        return -ret;
    }
    ret = tcm_exch_fabric(ctx.fabrics[CID_FRAME],
        ctx.fabrics[CID_FRAME]->mr_info.ptr, 1, ctx.fabrics[CID_FRAME]->mr,
        ((uint8_t *) ctx.fabrics[CID_FRAME]->mr_info.ptr) + 1, 1, 
        ctx.fabrics[CID_FRAME]->mr, ctx.cur_peers[CID_FRAME],
        &ctx.timeout, &err);
    if (ret < 0)
    {
        lp__log_error("Timed out waiting for message on frame channel");
        return -ret;
    }

    activate_proxy();
}