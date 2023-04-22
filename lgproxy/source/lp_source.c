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
#include "lp_source.h"

#include "tcm_fabric.h"
#include "tcm_errno.h"
#include "tcm_udp.h"
#include "tcmu.h"
#include "tcm.h"

#include "lp_lgmp_client.h"
#include "lp_version.h"
#include "lp_config.h"
#include "lp_msg.h"

#include <pthread.h>

#define CID_DATA     0
#define CID_FRAME    1

#define SSAI         struct sockaddr_in *

#define MR_SEND_OFFSET      0
#define MR_SEND_MAX         lp_get_page_size()

#define MR_RECV_OFFSET      (2 * lp_get_page_size())
#define MR_RECV_MAX         lp_get_page_size()

#define MR_CUR_DATA_OFFSET  (3 * lp_get_page_size())

#define SEND_Q_LEN 20

#define CTX_ID_CUR      0
#define CTX_ID_FRAME    1
#define CTX_ID_FRAME_NOTIFY 2

volatile int8_t exit_flag = 0;

struct proxy_exit {
    int ret;
};

enum { STATE_NONE, STATE_RECV, STATE_READY, STATE_SEND, STATE_SEND_COMPLETE, STATE_STANDBY, STATE_WAIT };

void exitHandler(int dummy)
{
    if (exit_flag == 1)
    {
        lp__log_info("Force quitting...");
        exit(0);
    }
    exit_flag = 1;
}

void waitHandler(int dummy)
{

}

struct ctx {
    tcm_sock            beacon_sock;    /* UDP socket for initial startup */
    tcm_fabric          * fabrics[2];   /* Fabric channels (frame + data) */
    fi_addr_t           cur_peers[2];   /* Fabric addresses of the sink (frame + data) */
    lp_lgmp_client_ctx  * lgmp_client;  /* Items required for LGMP interop */
    void                * sock_buf;     /* Socket buffer for initial startup */
    tcm_time            timeout;        /* Global network I/O timeout */

    /* Data flow states */

    uint8_t             client_state;   /* Client state */
    uint8_t             mouse_tx;       /* Mouse message (metadata) */
    uint8_t             mouse_shape_tx; /* Mouse shape data only */
    uint8_t             frame_tx;       /* Frame message (metadata) */
    uint8_t             frame_data_tx;  /* Raw frame data only */
    uint8_t             data_rx;        /* Receive on data channel (any) */
    uint8_t             frame_rx;       /* Receive on frame channel (any) */

    uint8_t             cli_buf_stat;   /* Bitfield for which buffers are ready to receive */
};

struct ctx ctx;
struct proxy_exit exit_info;
lp_config_opts opts;

#define return_threaded(code) {exit_info.ret = code; return 0;}

void zero_ctx()
{
    ctx.beacon_sock = -1;
    ctx.fabrics[CID_DATA] = NULL;
    ctx.fabrics[CID_FRAME] = NULL;
    ctx.cur_peers[CID_DATA] = FI_ADDR_UNSPEC;
    ctx.cur_peers[CID_FRAME] = FI_ADDR_UNSPEC;
    ctx.lgmp_client = NULL;
    ctx.sock_buf = NULL;
}

void cleanup_ctx()
{
    if (ctx.beacon_sock > 0)
        close(ctx.beacon_sock);
    if (ctx.sock_buf)
        free(ctx.sock_buf);
    if (ctx.lgmp_client)
    {
        if (ctx.lgmp_client->ptr_q)
            lgmpClientUnsubscribe(&ctx.lgmp_client->ptr_q);
        if (ctx.lgmp_client->frame_q)
            lgmpClientUnsubscribe(&ctx.lgmp_client->frame_q);
        if (ctx.lgmp_client->cli)
            lgmpClientFree(&ctx.lgmp_client->cli);
        if (ctx.lgmp_client->mem)
            munmap(ctx.lgmp_client->mem, ctx.lgmp_client->mem_size);
    }
    if (ctx.fabrics[CID_DATA])
        tcm_destroy_fabric(ctx.fabrics[CID_DATA], 1);
    if (ctx.fabrics[CID_FRAME])
        tcm_destroy_fabric(ctx.fabrics[CID_FRAME], 1);
    zero_ctx();
}

int reset_proxy()
{
    int ret;
    if (ctx.lgmp_client)
    {
        if (ctx.lgmp_client->ptr_q)
            lgmpClientUnsubscribe(&ctx.lgmp_client->ptr_q);
        if (ctx.lgmp_client->frame_q)
            lgmpClientUnsubscribe(&ctx.lgmp_client->frame_q);
        if (ctx.lgmp_client->cli)
            lgmpClientFree(&ctx.lgmp_client->cli);
        if (ctx.lgmp_client->mem)
            munmap(ctx.lgmp_client->mem, ctx.lgmp_client->mem_size);
        ctx.lgmp_client = NULL;
    }
    ret = fi_av_remove(ctx.fabrics[CID_DATA]->av, &ctx.cur_peers[CID_DATA], 1, 0);
    if (ret < 0)
    {
        lp__log_error("Fabric reset failed: %s", fi_strerror(-ret));
        return ret;
    }
    ret = fi_av_remove(ctx.fabrics[CID_FRAME]->av, &ctx.cur_peers[CID_FRAME], 1, 0);
    if (ret < 0)
    {
        lp__log_error("Fabric reset failed: %s", fi_strerror(-ret));
        return ret;
    }
    ctx.cur_peers[CID_DATA]  = FI_ADDR_UNSPEC;
    ctx.cur_peers[CID_FRAME] = FI_ADDR_UNSPEC;
    return 0;
}

int init_client()
{
    /* Initialize LGMP client */
    ssize_t ret;
    lp__log_info("Creating LGMP client on %s", opts.mem_path);
    ret = lp_create_lgmp_client(opts.mem_path, &ctx.timeout, &ctx.lgmp_client);
    if (ret < 0)
    {
        tcm__log_error("Failed to create LGMP client: %s",
                        strerror(-ret));
        return ret;
    }

    /* Register memory with RDMA driver */
    ret = fi_mr_reg(ctx.fabrics[CID_FRAME]->domain, ctx.lgmp_client->mem,
                    ctx.lgmp_client->mem_size, 
                    FI_READ | FI_WRITE, 0, 0, 0, 
                    &ctx.lgmp_client->mr, NULL);
    if (ret < 0)
    {
        tcm__log_error("Could not register SHM region for RDMA access: %s",
                        fi_strerror(-ret));
        return ret;
    }

    /* Request first frame (check everything works) */
    lp_lgmp_msg frame;
    lp__log_info("Requesting initial frame from LGMP host app");
    ret = lp_request_lgmp_frame(ctx.lgmp_client->cli, &ctx.lgmp_client->frame_q,
                                &frame, &ctx.timeout);
    if (ret < 0)
    {
        tcm__log_error("Failed to request LGMP frame: %s",
                        strerror(-ret));
        return ret;
    }
    

    /* Parse metadata. Source: LookingGlass/client/src/main.c */
    KVMFR * base_info = (KVMFR *) ctx.lgmp_client->udata;
    KVMFRRecord_VMInfo * vm_info = calloc(1, sizeof(*vm_info) + 64);
    KVMFRRecord_OSInfo * os_info = calloc(1, sizeof(*os_info) + 64);
    if (!vm_info || !os_info)
    {
        tcm__log_fatal("Memory allocation failed");
        return -ENOMEM;
    }
    
    size_t size_tmp = ctx.lgmp_client->udata_size - sizeof(KVMFR);
    uint8_t * p = (uint8_t *)(base_info + 1);
    while (size_tmp >= sizeof(KVMFRRecord))
    {
        KVMFRRecord * record = (KVMFRRecord *) p;
        p        += sizeof(*record);
        size_tmp -= sizeof(*record);
        if (record->size > size_tmp)
        {
            tcm__log_warn("KVMFRRecord size %lu invalid", record->size);
            break;
        }
        lp__log_trace("KVMFRRecord Type: %d, size: %d", record->type, record->size);
        switch (record->type)
        {
            case KVMFR_RECORD_VMINFO:
                memcpy(vm_info, p, record->size);
                break;
            case KVMFR_RECORD_OSINFO:
                memcpy(os_info, p, record->size);
                break;
            default:
                tcm__log_warn("Unknown KVMFR record type: %d", record->type);
                break;
        }
        p         += record->size;
        size_tmp -= record->size;
    }

    /* Create metadata message */
    lp_msg_metadata * mtd = ctx.fabrics[CID_DATA]->mr_info.ptr;
    memset(mtd, 0, sizeof(*mtd));
    mtd->hdr.id         = LP_MSG_METADATA;
    mtd->lp_major       = LP_VERSION_MAJOR;
    mtd->lp_minor       = LP_VERSION_MINOR;
    mtd->lp_patch       = LP_VERSION_PATCH;
    mtd->kvmfr_cores    = vm_info->cores;
    mtd->kvmfr_sockets  = vm_info->sockets;
    mtd->kvmfr_threads  = vm_info->cpus;
    mtd->kvmfr_flags    = base_info->features;
    mtd->kvmfr_os_id    = os_info->os;
    mtd->kvmfr_fmt      = frame.frame->type;
    mtd->width          = frame.frame->frameWidth;
    mtd->height         = frame.frame->frameHeight;
    mtd->pitch          = frame.frame->pitch;
    mtd->stride         = frame.frame->stride;
    memcpy(mtd->kvmfr_uuid, vm_info->uuid, 16);
    strncpy(mtd->kvmfr_capture, vm_info->capture, 31);
    strncpy(mtd->cpu_model, vm_info->model, 63);
    strncpy(mtd->os_name, os_info->name, 31);
    
    /*  Send metadata message */
    struct fi_cq_err_entry err = {0};
    ret = tcm_tsend_fabric(ctx.fabrics[CID_DATA], mtd, sizeof(*mtd),
                           ctx.fabrics[CID_DATA]->mr, ctx.cur_peers[CID_DATA],
                           LP_TAG_SYS_INFO, mtd, &ctx.timeout);
    if (ret < 0)
    {
        lp__log_error("Fabric post send failed: %s", fi_strerror(-ret));
        return ret;
    }
    ret = tcm_wait_fabric(ctx.fabrics[CID_DATA]->tx_cq, &ctx.timeout, &err);
    if (ret < 0)
    {
        lp__log_error("Fabric send failed: %s", fi_strerror(-ret));
        return ret;
    }

    free(vm_info);
    free(os_info);
    lp__log_debug("Client init complete");
    return 0;
}

int check_cli_state()
{
    return 0;
}

int post_rx()
{
    tcm_fabric * f_fab = ctx.fabrics[CID_FRAME];
    fi_addr_t f_peer   = ctx.cur_peers[CID_FRAME];
    tcm_fabric * d_fab = ctx.fabrics[CID_DATA];
    fi_addr_t d_peer   = ctx.cur_peers[CID_DATA];
    int procd = 0;
    int ret;

    if (ctx.frame_rx == STATE_NONE)
    {
        ret = tcm_trecv_fabric(f_fab, f_fab->mr_info.ptr, sizeof(lp_msg_state),
                               f_fab->mr, f_peer, LP_TAG_STAT, LP_TAG_MASK, 0, 
                               &ctx.timeout);
        if (ret < 0)
        {
            return ret;
        }
        ctx.frame_rx = STATE_WAIT;
        procd++;
    }

    if (ctx.data_rx == STATE_NONE)
    {
        ret = tcm_trecv_fabric(d_fab, d_fab->mr_info.ptr, sizeof(lp_msg_state),
                               d_fab->mr, d_peer, LP_TAG_STAT, LP_TAG_MASK, 0, 
                               &ctx.timeout);
        if (ret < 0)
        {
            return ret;
        }
        ctx.data_rx = STATE_WAIT;
        procd++;
    }

    return procd;
}

int check_rx(struct fi_cq_err_entry * err, tcm_time * timing)
{
    tcm_fabric * f_fab = ctx.fabrics[CID_FRAME];
    tcm_fabric * d_fab = ctx.fabrics[CID_DATA];
    int ret;

    if (ctx.frame_rx == STATE_WAIT)
    {
        ret = tcm_wait_fabric(f_fab->rx_cq, timing, err);
        if (ret > 0)
        {
            /* Client should not send any valid messages on the frame channel */
            ctx.frame_rx = STATE_NONE;
        }
        if (ret < 0 && ret != -EAGAIN && ret != -ETIMEDOUT)
        {
            return ret;
        }
    }

    if (ctx.data_rx == STATE_WAIT)
    {
        ret = tcm_wait_fabric(d_fab->rx_cq, timing, err);
        if (ret > 0)
        {
            lp_msg_state * stat = (lp_msg_state *) d_fab->mr_info.ptr;
            if (stat->hdr.id != LP_MSG_STAT)
            {
                lp__log_trace("Peer state: %lu", stat->peer_state);
                ctx.client_state = stat->peer_state;
            }
            ctx.data_rx = STATE_NONE;

            /* Immediately post a new receive */
            ret = post_rx();
            if (ret < 0)
                return ret;
        }
        if (ret < 0 && ret != -EAGAIN && ret != -ETIMEDOUT)
        {
            return ret;
        }
    }

    return 0;
}

void * wait_sock(void * arg)
{
    int ret;
    while (!exit_flag)
    {
        errno = 0;
        struct sockaddr_storage other_peer;
        socklen_t other_peer_len = sizeof(other_peer);
        ret = recvfrom(ctx.beacon_sock, ctx.sock_buf, 511, MSG_NOSIGNAL, 
                        (struct sockaddr *) &other_peer, &other_peer_len);
        if (ret <= 0)
        {
            switch (errno)
            {
                case 0:
                case EAGAIN:
                    break;
                case EINTR:
                    return 0;
                default:
                    lp__log_warn("Socket error: %s", strerror(errno));
                    lp__log_warn("Disabling socket checks");
                    opts.no_socket_check = 1;
            }
        }
        else
        {
            /*  Decode the client header and just check it's a valid TCM
                message
            */
            tcm_msg_header * hdr = ctx.sock_buf;
            int r2 = tcm_msg_verify(hdr, ret);
            if (r2 < 0)
            {
                tcm__log_debug("Invalid message of size %lu received on "
                                "beacon: %s", ret, strerror(r2));
                break;
            }

            /* Tell the other client we're busy */
            tcm_msg_server_status * sstat = \
                (tcm_msg_server_status *)((uint8_t *) ctx.sock_buf + 512);
            sstat->common.id = TCM_MSG_SERVER_STATUS;
            tcm_msg_init(sstat, hdr->token);
            sstat->retcode = TCM_EBUSY;
            errno = 0;
            ret = sendto(ctx.beacon_sock, sstat, sizeof(*sstat), 0,
                            (struct sockaddr *) &other_peer, other_peer_len);
            if (ret < 0)
            {
                switch (errno)
                {
                    case 0:
                    case EAGAIN:
                        break;
                    case EINTR:
                        lp__log_info("Received interrupt, socket thread exiting");
                        return 0;
                    default:
                        lp__log_warn("Socket error: %s", strerror(errno));
                        lp__log_warn("Disabling socket checks");
                        return 0;
                }
            }
        }
    }
    return 0;
}

void reset_data_state()
{
    ctx.data_rx = STATE_NONE;
    ctx.frame_rx = STATE_NONE;
    ctx.frame_data_tx = STATE_NONE;
    ctx.frame_tx = STATE_NONE;
    ctx.mouse_tx = STATE_NONE;
    ctx.mouse_shape_tx = STATE_NONE;
    ctx.client_state = 0;
    ctx.cli_buf_stat = 0;
}

void * activate_proxy(void * arg)
{
    ssize_t ret;
    struct fi_cq_err_entry err;
    ret = init_client();
    if (ret < 0)
        return_threaded(ret);
    
    /*  RDMA Memory region layout: 
        [Peer Status x 1] [Frame Metadata x 2] [Cursor Metadata x 20] 
        [Cursor Data x 1]
    */
    uint8_t * base_ptr        = ctx.fabrics[CID_DATA]->mr_info.ptr;
    lp_msg_kvmfr * kvmfr_tx   = (lp_msg_kvmfr *)  (base_ptr + sizeof(lp_msg_state));
    lp_msg_cursor * cursor_tx = (lp_msg_cursor *) (kvmfr_tx + LGMP_Q_FRAME_LEN);
    lp_msg_state * state_tx   = (lp_msg_state *)  (cursor_tx + LGMP_Q_POINTER_LEN);
    void * cursor_shape_tx    =                   (state_tx + 2);
    lp_lgmp_client_ctx * lc   = ctx.lgmp_client;
    
    // LGMP_STATUS lg_ret;
    tcm_time once = {
        .ts.tv_sec = 0,
        .ts.tv_nsec = 0,
        .interval = 0,
        .delta = 1
    };

    reset_data_state();
    struct timespec frame_tx_start, frame_tx_end;

    /* Time since the last update was received from LGMP */
    // struct timespec last_update;
    lp__log_debug("Starting proxy loop");
    while (1)
    {
        /* Throttle the polling rate if the client is in standby mode */
        if (ctx.client_state == 0)
            if (opts.poll_interval < 0.001f) /* <1ms sleep duration */
                tcm_fsleep(0.05f); /* ... wait for 50ms */
            else
                tcm_fsleep(opts.poll_interval * 10.0f);
        else
            tcm_fsleep(opts.poll_interval);
        
        /*  Post new receives on data and frame channels. */


        ret = post_rx();
        if (ret < 0 && ret != -EAGAIN && ret != -ETIMEDOUT)
        {
            lp__log_error("Receive post failed: %s", fi_strerror(-ret));
            return_threaded(ret);
        }

        ret = check_rx(&err, &once);
        if (ret < 0 && ret != -EAGAIN && ret != -ETIMEDOUT)
        {
            lp__log_error("Receive check failed: %s", fi_strerror(-ret));
            return_threaded(ret);
        }

        /* Request frame from LGMP server */

        lp_lgmp_msg frame_info;
        if (ctx.frame_tx == STATE_NONE && ctx.frame_data_tx == STATE_NONE)
        {
            frame_info.frame = NULL;
            ret = lp_request_lgmp_frame(lc->cli, &lc->frame_q, &frame_info,
                                        &once);
            if (ret >= 0 && frame_info.frame)
            {   
                ctx.frame_tx = STATE_READY;
                ctx.frame_data_tx = STATE_READY;
            }
        }

        if (ctx.frame_tx == STATE_READY)
        {
            kvmfrframe_to_lp_msg_kvmfr(frame_info.frame, kvmfr_tx);
            ret = tcm_tsend_fabric(ctx.fabrics[CID_DATA], kvmfr_tx,
                    sizeof(*kvmfr_tx), ctx.fabrics[CID_DATA]->mr,
                    ctx.cur_peers[CID_DATA], LP_TAG_FRAME_INFO, (uintptr_t *) LP_TAG_FRAME_INFO, &once);
            if (ret < 0 && ret != -EAGAIN && ret != -ETIMEDOUT)
            {
                lp__log_error("Fabric send error: %s", fi_strerror(-ret));
                return_threaded(ret);
            }
            else if (ret == 0)
            {
                lp__log_trace("Frame metadata sent");
                ctx.frame_tx = STATE_SEND;
                ctx.cli_buf_stat &= ~LP_BUF_FRAME_INFO;
            }
        }

        if (ctx.frame_data_tx == STATE_READY)
        {
            void * tex = ((uint8_t *) frame_info.frame) 
                         + frame_info.frame->offset + FB_WP_SIZE;
            ret = tcm_tsend_fabric(ctx.fabrics[CID_FRAME], tex,
                    frame_info.frame->pitch * frame_info.frame->frameHeight,
                    ctx.fabrics[CID_FRAME]->mr, ctx.cur_peers[CID_FRAME],
                    LP_TAG_FRAME_DATA, (uintptr_t *) LP_TAG_FRAME_DATA, &once);
            if (ret < 0 && ret != -EAGAIN && ret != -ETIMEDOUT)
            {
                lp__log_error("Fabric send error: %s", fi_strerror(-ret));
                return_threaded(ret);
            }
            else if (ret == 0)
            {
                lp__log_trace("Frame texture data sent");
                ctx.frame_data_tx = STATE_SEND;
                ctx.cli_buf_stat &= ~LP_BUF_FRAME_DATA;
                clock_gettime(CLOCK_MONOTONIC, &frame_tx_start);
            }
        }
        
        /* Request cursor from LGMP server */

        lp_lgmp_msg cursor_info;
        KVMFRCursor cur;
        cursor_info.cursor = &cur;
        if (ctx.mouse_tx == STATE_NONE && ctx.mouse_shape_tx == STATE_NONE)
        {
            ret = lp_request_lgmp_cursor(lc->cli, &lc->ptr_q, &cursor_info,
                                         cursor_shape_tx, &once);
            if (ret >= 0 && frame_info.frame)
            {   
                ctx.mouse_tx = STATE_READY;
                if (cursor_info.flags & CURSOR_FLAG_SHAPE)
                    ctx.mouse_shape_tx = STATE_READY;
            }
        }

        if (ctx.mouse_tx == STATE_READY)
        {
            kvmfrcursor_to_lp_msg_cursor(cursor_info.cursor, cursor_info.flags,
                                         cursor_tx);
            ret = tcm_tsend_fabric(ctx.fabrics[CID_DATA], cursor_tx,
                sizeof(*cursor_tx), ctx.fabrics[CID_DATA]->mr, 
                ctx.cur_peers[CID_DATA], LP_TAG_CURSOR_INFO, (uintptr_t *) LP_TAG_CURSOR_INFO, &once);
            if (ret < 0 && ret != -EAGAIN && ret != -ETIMEDOUT)
            {
                lp__log_error("Fabric send error: %s", fi_strerror(-ret));
                return_threaded(ret);
            }
            else if (ret == 0)
            {
                lp__log_trace("Cursor metadata sent");
                ctx.mouse_tx = STATE_SEND;
                ctx.cli_buf_stat &= ~LP_BUF_CURSOR_INFO;
            }
        }

        if (ctx.mouse_shape_tx == STATE_READY)
        {
            ret = tcm_tsend_fabric(ctx.fabrics[CID_DATA], cursor_shape_tx,
                cursor_info.cursor->pitch * cursor_info.cursor->height,
                ctx.fabrics[CID_DATA]->mr, ctx.cur_peers[CID_DATA], 
                LP_TAG_CURSOR_DATA, (uintptr_t *) LP_TAG_CURSOR_DATA, &once);
            if (ret < 0 && ret != -EAGAIN && ret != -ETIMEDOUT)
            {
                lp__log_error("Fabric send error: %s", fi_strerror(-ret));
                return_threaded(ret);
            }
            else if (ret == 0)
            {
                lp__log_trace("Cursor data sent");
                ctx.mouse_shape_tx = STATE_SEND;
                ctx.cli_buf_stat &= ~LP_BUF_CURSOR_DATA;
            }
        }

        /* Check pending send operations */

        if (ctx.mouse_tx == STATE_SEND || ctx.mouse_shape_tx == STATE_SEND
            || ctx.frame_tx == STATE_SEND)
        {
            err.op_context = 0;
            ret = tcm_wait_fabric(ctx.fabrics[CID_DATA]->tx_cq, &once, &err);
            if (ret < 0 && ret != -EAGAIN && ret != -ETIMEDOUT)
            {
                lp__log_error("Fabric wait error: %s", fi_strerror(-ret));
                return_threaded(ret);
            }
            if (ret > 0)
            {
                uint64_t tag = ((uint64_t) err.op_context) & ~LP_TAG_MASK;
                switch (tag)
                {
                    case LP_TAG_CURSOR_INFO:
                        ctx.mouse_tx = STATE_NONE;
                        break;
                    case LP_TAG_CURSOR_DATA:
                        ctx.mouse_shape_tx = STATE_NONE;
                        break;
                    case LP_TAG_FRAME_INFO:
                        ctx.frame_tx = STATE_NONE;
                        break;
                    default:
                        lp__log_error("Invalid send type %p", err.op_context);
                        return_threaded(-EINVAL);
                }
            }
        }

        if (ctx.frame_data_tx == STATE_SEND)
        {
            err.op_context = 0;
            ret = tcm_wait_fabric(ctx.fabrics[CID_FRAME]->tx_cq, &once, &err);
            if (ret < 0 && ret != -EAGAIN && ret != -ETIMEDOUT)
            {
                lp__log_error("Fabric wait error: %s", fi_strerror(-ret));
                return_threaded(ret);
            }
            if (ret > 0)
            {
                uint64_t tag = ((uint64_t) err.op_context) & ~LP_TAG_MASK;
                switch (tag)
                {
                    case LP_TAG_FRAME_DATA:
                        lgmpClientMessageDone(lc->frame_q);
                        clock_gettime(CLOCK_MONOTONIC, &frame_tx_end);
                        struct timespec diff;
                        float f = 0;
                        tsDiff(&diff, &frame_tx_end, &frame_tx_start);
                        f += diff.tv_sec * 1000;
                        f += diff.tv_nsec / 1000000;
                        lp__log_trace("Frame sent in %.3f ms", f);
                        ctx.frame_data_tx = STATE_NONE;
                        break;
                    default:
                        lp__log_error("Invalid send type %p", tag);
                        return_threaded(-EINVAL);
                }
            }
        }
    }
    return 0;
}

int main(int argc, char ** argv)
{
    lp_set_log_level(); 
    signal(SIGINT, exitHandler);
    ssize_t ret;
    
    ret = lp_load_cmdline(argc, argv, 1, &opts);
    if (ret < 0)
    {
        lp__log_fatal("Failed to load command line options: %s", strerror(-ret));
        lp_print_usage(1);
        return -ret;
    }

    /* Check the fabric is supported and set the transport ID */
    int tid = lp_prov_name_to_tid(opts.transport);
    if (tid < 0)
    {
        lp__log_fatal("Transport provider %s unsupported: %s", opts.transport,
                      strerror(-tid));
        return tid;
    }

    zero_ctx();

    ctx.timeout.delta = 1;
    ctx.timeout.interval = opts.poll_interval;
    ctx.timeout.ts.tv_sec = 3;
    ctx.timeout.ts.tv_nsec = 0;

    /* Create beacon */

    lp__log_info("Creating UDP channel");
    ret = tcm_setup_udp(opts.bind_addr, TCM_SOCK_MODE_SYNC, &ctx.beacon_sock);
    if (ret < 0)
    {
        lp__log_fatal("Beacon socket creation failed: %s", strerror(-ret));
        return -ret;
    }

    /*  Create fabric channels */

    lp__log_info("Creating fabric channels");
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
                               MAX_POINTER_SIZE * POINTER_SHAPE_BUFFERS);
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

    ret = tcm_set_timeout_udp(ctx.beacon_sock, 3e3, 3e6);
    if (ret < 0)
    {
        lp__log_fatal("Could not set socket timeout");
        cleanup_ctx();
        return -ret;
    }

    /* Answer messages sent to the beacon */
    lp__log_info("Ready, waiting for clients");
    tcm_msg_header * hdr = ctx.sock_buf;
    struct sockaddr_storage peer;
    while (1)
    {
        errno = 0;
        socklen_t peer_len = sizeof(peer);
        ret = recvfrom(ctx.beacon_sock, ctx.sock_buf, 1023, MSG_NOSIGNAL,
                       (struct sockaddr *) &peer, &peer_len);
        switch (errno)
        {
            case 0:
                break;
            case EAGAIN:
                continue;
            case EINTR:
                if (exit_flag == 1)
                {
                    lp__log_info("Quitting...");
                    cleanup_ctx();
                    return 0;
                }
                continue;
            default:
                lp__log_error("Socket error: %s", strerror(errno));
                return errno;
        }
        if (tcm_msg_verify(hdr, ret) < 0)
        {
            lp__log_error("Client sent invalid message");
            continue;
        }
        lp__log_debug("Received message type %lu, size %lu", hdr->id, ret);
        void * send_ptr = NULL;
        size_t send_size = 0;

        /*  Metadata messages.
            Messages that require state changes should be processed outside
            of this switch/case 
        */
        switch (hdr->id)
        {
            case TCM_MSG_CLIENT_PING:
                tcm_msg_client_ping * ping = ctx.sock_buf;
                tcm_msg_server_status * sstat = (tcm_msg_server_status *) (ping + 1);
                tcm_msg_init(ctx.sock_buf, ping->common.token);
                sstat->common.id = TCM_MSG_SERVER_STATUS;
                tcm_msg_init(sstat, ping->common.token);
                if (sstat->version.major != ping->version.major
                    || sstat->version.minor != ping->version.minor
                    || sstat->version.patch != ping->version.patch)
                {
                    sstat->retcode = TCM_ENOTSUP;
                }
                else
                {
                    sstat->retcode = 0;
                }
                send_ptr  = sstat;
                send_size = sizeof(tcm_msg_server_status);
                break;
            case TCM_MSG_METADATA_REQ:
                tcm_msg_metadata_req * req = ctx.sock_buf;
                tcm_msg_metadata_resp * resp = (tcm_msg_metadata_resp *) (req + 1);    
                tcm_addr_inet * net = (void *) resp->addr;
                resp->common.id = TCM_MSG_METADATA_RESP;
                resp->common.magic = TCM_MAGIC;
                resp->common.token = req->common.token;
                resp->addr_fmt = TCM_AF_INET;
                resp->addr_len = sizeof(*net);
                resp->fabric_major = FI_MAJOR(opts.fabric_ver);
                resp->fabric_minor = FI_MINOR(opts.fabric_ver);
                memset(resp->pad, 0, sizeof(resp->pad));
                switch (req->cid)
                {
                    case CID_DATA:
                        net->addr = ((SSAI) opts.data_addr)->sin_addr.s_addr;
                        net->port = ((SSAI) opts.data_addr)->sin_port;
                        resp->tid = ctx.fabrics[CID_DATA]->transport_id;
                        break;
                    case CID_FRAME:
                        net->addr = ((SSAI) opts.frame_addr)->sin_addr.s_addr;
                        net->port = ((SSAI) opts.frame_addr)->sin_port;
                        resp->tid = ctx.fabrics[CID_FRAME]->transport_id;
                        break;
                    default:
                        resp->addr_len = 0;
                        break;
                }
                if (resp->addr_len == 0)
                {
                    tcm_msg_server_status * stat = (tcm_msg_server_status *) resp;
                    stat->version.major = TCM_VERSION_MAJOR;
                    stat->version.minor = TCM_VERSION_MINOR;
                    stat->version.patch = TCM_VERSION_PATCH;
                    stat->retcode = TCM_EINVAL;
                    send_ptr  = sstat;
                    send_size = sizeof(tcm_msg_server_status);
                }
                else
                {
                    send_ptr  = resp;
                    send_size = sizeof(tcm_msg_metadata_resp) + resp->addr_len;
                }
                break;
            case TCM_MSG_CONN_REQ:
                send_ptr = NULL;
                send_size = TCM_MSG_CONN_REQ;
                break;
            default:
                send_ptr = NULL;
                send_size = 0;
                break;
        }

        /* If a response message was prepared, send it */
        if (send_ptr)
        {
            ret = sendto(ctx.beacon_sock, send_ptr, send_size, MSG_CONFIRM, 
                         (struct sockaddr *) &peer, sizeof(peer));
            if (ret < 0)
            {
                lp__log_error("Send failed: %s", strerror(errno));
                continue;
            }
        }

        /* Something else must be done before sending a message */
        else if (send_size == TCM_MSG_CONN_REQ)
        {
            int client_rcode = 0;
            tcm_msg_conn_req * req = ctx.sock_buf;
            /* For now, only AF_INET supported */
            if (req->addr_fmt != TCM_AF_INET)
                client_rcode = TCM_EINVAL;
            /* Verify the transport ID exists */
            else if (req->tid <= TCM_TID_INVALID || req->tid >= TCM_TID_MAX)
                client_rcode = TCM_EINVAL;
            /* Check the fabric versions match */
            else if (req->fabric_major != FI_MAJOR(opts.fabric_ver)
                     || req->fabric_minor != FI_MINOR(opts.fabric_ver))
                client_rcode = TCM_ENOTSUP;
            /* Check the channel ID is correct */
            else if (req->cid != CID_DATA && req->cid != CID_FRAME)
                client_rcode = TCM_ESRCH;
            /* Everything checks out, add the peer */
            else
            {
                tcm_addr_inet * inet = (void *) req->addr;
                lp__log_debug("Adding peer on CID %d", req->cid);
                lp__log_debug("Addr data: %d %d", ntohl(inet->addr), ntohs(inet->port));
                struct sockaddr_in new_peer;
                memset(&new_peer, 0, sizeof(new_peer));
                new_peer.sin_addr.s_addr = inet->addr;
                new_peer.sin_port = inet->port;
                new_peer.sin_family = AF_INET;
                /* Remove the old peer before connecting a new one */
                if (ctx.cur_peers[req->cid] != FI_ADDR_UNSPEC)
                {
                    lp__log_debug("Removing old peer");
                    ret = tcmu_remove_peer(ctx.fabrics[req->cid], 
                                           ctx.cur_peers[req->cid]);
                    if (ret < 0)
                    {
                        client_rcode = TCM_ENOSPC;
                    }
                }
                ctx.cur_peers[req->cid] = FI_ADDR_UNSPEC;
                ret = tcmu_add_peer(ctx.fabrics[req->cid],
                                    (struct sockaddr *) &new_peer,
                                    &ctx.cur_peers[req->cid]);
                if (ret < 0)
                {
                    lp__log_error("Peer add failed: %s", fi_strerror(-ret));
                    client_rcode = TCM_EIO;
                }
                else
                    client_rcode = 0;
            }
            if (client_rcode != 0)
                lp__log_debug("Peer add failed on CID %d: %s", req->cid,
                              strerror(tcm_err_to_sys(client_rcode)));
            else
                lp__log_debug("Peer added on CID %d", req->cid);

            tcm_msg_server_status * sstat = (tcm_msg_server_status *) 
                ((uint8_t *)(req + 1) + req->addr_len);
            sstat->common.id = TCM_MSG_SERVER_STATUS;
            tcm_msg_init(sstat, req->common.token);
            sstat->retcode = client_rcode;
            ret = sendto(ctx.beacon_sock, sstat, sizeof(*sstat), MSG_CONFIRM, 
                         (struct sockaddr *) &peer, sizeof(peer));
            if (ret < 0)
            {
                lp__log_error("Send failed: %s", strerror(errno));
                continue;
            }
        }

        /*  Message invalid. Don't reply, it could be from another protocol */
        else
        {
            continue;
        }

        /*  Only start the frame capture once both fabric links are active */
        if (ctx.cur_peers[CID_DATA] == FI_ADDR_UNSPEC 
            || ctx.cur_peers[CID_FRAME] == FI_ADDR_UNSPEC)
                continue;

        /* Exchange small messages to verify the link works */
        struct fi_cq_err_entry err;
        ret = tcm_exch_fabric_rev(ctx.fabrics[CID_DATA],
            ctx.fabrics[CID_DATA]->mr_info.ptr, 1, ctx.fabrics[CID_DATA]->mr,
            ctx.fabrics[CID_DATA]->mr_info.ptr, 1, ctx.fabrics[CID_DATA]->mr, 
            ctx.cur_peers[CID_DATA], 
            &ctx.timeout, &err);
        if (ret < 0)
        {
            lp__log_error("Timed out waiting for message on data channel");
            continue;
        }
        ret = tcm_exch_fabric_rev(ctx.fabrics[CID_FRAME],
            ctx.fabrics[CID_FRAME]->mr_info.ptr, 1, ctx.fabrics[CID_FRAME]->mr,
            ((uint8_t *) ctx.fabrics[CID_FRAME]->mr_info.ptr) + 1, 1, 
            ctx.fabrics[CID_FRAME]->mr, ctx.cur_peers[CID_FRAME],
            &ctx.timeout, &err);
        if (ret < 0)
        {
            lp__log_error("Timed out waiting for message on frame channel");
            continue;
        }

        /* Create proxy thread */
        pthread_t main_thr;
        ret = pthread_create(&main_thr, NULL, activate_proxy, NULL);
        if (ret != 0)
        {
            lp__log_error("Thread creation failed: %s", strerror(ret));
            return ret;
        }

        /* Create socket waiting thread */
        pthread_t sock_thr;
        if (!opts.no_socket_check)
        {
            ret = pthread_create(&sock_thr, NULL, wait_sock, NULL);
            if (ret != 0)
            {
                lp__log_error("Thread creation failed: %s", strerror(ret));
                return ret;
            }
        }

        /* Wait for main thread to exit */
        ret = pthread_join(main_thr, NULL);
        if (ret != 0)
        {
            lp__log_error("Thread error: %s", strerror(ret));
            return ret;
        }

        /* Return control of socket to this thread */
        lp__log_debug("Proxy thread exited, reclaiming socket");
        ret = pthread_kill(sock_thr, SIGINT);
        if (ret != 0)
        {
            lp__log_error("Thread kill error: %s", strerror(ret));
            return ret;
        }
        ret = pthread_join(sock_thr, NULL);
        if (ret != 0)
        {
            lp__log_error("Thread error: %s", strerror(ret));
            return ret;
        }

        /* Reset the fabric and LGMP client to an unconnected state */
        lp__log_debug("Resetting proxy");
        ret = reset_proxy();
        if (ret < 0)
        {
            lp__log_error("Proxy resource state reset failed: %s", fi_strerror(-ret));
            return -ret;
        }

        lp__log_debug("Ready for new client");
    }
}
