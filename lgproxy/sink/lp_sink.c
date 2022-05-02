/*

    SPDX-License-Identifier: GPL-2.0-only

    Telescope Project 
    Looking Glass Proxy
    Sink Application

    Copyright (c) 2022 Matthew John McMullin

    This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA. 

*/

#include "lp_sink.h"
#include "version.h"

static const char * LP_USAGE_GUIDE_STR =                                \
"Looking Glass Proxy (LGProxy)\n"                                       \
"Copyright (c) 2022 Telescope Project Developers\n"                     \
"Matthew McMullin (@matthewjmc), Tim Dettmar (@beanfacts)\n"            \
"\n"                                                                    \
"Documentation: https://telescope-proj.github.io/lgproxy\n"             \
"Documentation also contains licenses for third party libraries\n"      \
"used by this project\n"                                                \
"\n"                                                                    \
"Options:\n"                                                            \
"   -h  Hostname or IP address to connect to\n"                         \
"   -p  Port or service name to connect to\n"                           \
"   -f  Shared memory or KVMFR file to use\n"                           \
"   -s  Size of the shared memory file - not required unless\n"         \
"       the file has not been created\n"                                \
"   -d  Delete the shared memory file on exit\n"                        \
"   -r  Polling interval in milliseconds\n"                             \
;

volatile int8_t flag = 0;

void exitHandler(int dummy)
{
    if (flag == 1)
    {
        lp__log_info("Force quitting...");
        exit(0);
    }
    flag = 1;
}

int main(int argc, char ** argv)
{
    signal(SIGINT, exitHandler);    
    PLPContext ctx = lpAllocContext();
    if (!ctx) {
        return - EINVAL;
    }
    int ret = 0;
    char * host = NULL;
    char * port = NULL;

    TrfMsg__MessageWrapper * msg = NULL;

    if (lpSetDefaultOpts(ctx))
    {
        lp__log_error("Unable to set default options");
        return -1;
    }
    
    int o;
    while ((o = getopt(argc, argv, "h:p:f:s:d:r:")) != -1)
    {
        switch (o)
        {
            case 'h':
                host = optarg;
                break;
            case 'p':
                port = optarg;
                break;
            case 'f':
                ctx->shm = optarg;
                break;
            case 's':
                ctx->ram_size = lpParseMemString(optarg);
                break;
            case 'd':
                ctx->opts.delete_exit = true;
                break;
            case 'r':
                lp__log_info("Poll Interval: %s", optarg);
                ctx->opts.poll_int = atoi(optarg);
                break;
            default:
            case '?':
                lp__log_fatal("Invalid argument -%c", optopt);
                fputs(LP_USAGE_GUIDE_STR, stdout);
                return EINVAL;
        }
    }


    if (!host || !port || !ctx->shm)
    {
        fputs(LP_USAGE_GUIDE_STR, stdout);
        lp__log_trace("Invalid Arguments");
        return EINVAL;
    }

    lpSetLPLogLevel();  // Set lgproxy log level
    lpSetTRFLogLevel(); // Set libtrf log level

    lp__log_info("Connecting to %s:%s", host,port);
    if ((ret = lpTrfClientInit(ctx, host, port)) < 0)
    {
        lp__log_error("Unable to initialize trf client");
        return -1;
    }

    // Get server version
    LpMsg__MessageWrapper * wrapper = NULL;
    ret = trfFabricRecv(ctx->lp_client.client_ctx,
                        &ctx->lp_client.client_ctx->xfer.fabric->msg_mem,
                        trfMemPtr(&ctx->lp_client.client_ctx->xfer.fabric->msg_mem),
                        trfMemSize(&ctx->lp_client.client_ctx->xfer.fabric->msg_mem),
                        ctx->lp_client.client_ctx->xfer.fabric->peer_addr,
                        ctx->lp_client.client_ctx->opts);
    if (ret < 0)
    {
        lp__log_error("Message receive failed: %s", fi_strerror(-ret));
        return -1;
    }
    int s = trfMsgGetPackedLength((trfMemPtr(&ctx->lp_client.client_ctx->xfer.fabric->msg_mem)));
    lp__log_trace("Packed Length: %d", s);
    ret = trfMsgUnpackProtobuf((ProtobufCMessage **) &wrapper,
                                (const ProtobufCMessageDescriptor *)
                                &lp_msg__message_wrapper__descriptor, s,
                                trfMsgGetPayload(
                                    trfMemPtr(&ctx->lp_client.client_ctx->xfer.fabric->msg_mem)
                                ));
    if (ret < 0)
    {
        lp__log_error("Message unpack failed: %s", strerror(-ret));
        return -1;
    }
    if (strncmp(wrapper->build_version->lg_version, LG_BUILD_VERSION, strlen(LG_BUILD_VERSION)) != 0
        || strncmp(wrapper->build_version->lp_version, LP_BUILD_VERSION, strlen(LP_BUILD_VERSION) != 0))
    {
        lp__log_error("Incorrect Version");
        lp__log_error("Server LG Version: %s Client LG Version: %s", 
                wrapper->build_version->lg_version,
                wrapper->build_version->lg_version);
        lp__log_error("Server LP Version: %s Client LP Version: %s", 
                wrapper->build_version->lp_version,
                wrapper->build_version->lp_version);
    }
    lp_msg__message_wrapper__free_unpacked(wrapper, NULL);
    wrapper = NULL;
    lp__log_info("Looking Glass Proxy Build: %s", LP_BUILD_VERSION);
    lp__log_info("Looking Glass Build: %s", LG_BUILD_VERSION);

    PTRFDisplay displays;
    ret = trfGetServerDisplays(ctx->lp_client.client_ctx, &displays);
    if (ret < 0)
    {
        lp__log_error("Unable to get servers display list");
        return -1;
    }

    // If user did not specify ramsize
    if (!ctx->ram_size)
    {
        ctx->ram_size = lpCalcFrameSizeNeeded(displays);
        lp__log_trace("Size needed for display: %d", ctx->ram_size);
    }

    lp__log_trace("Server Display List");
    lp__log_trace("---------------------------------------------");
    for (PTRFDisplay tmp = displays; tmp != NULL; tmp = tmp->next)
    {
        lp__log_trace("Display id: %d, Display name: %s", tmp->id, tmp->name);
        lp__log_trace("Resolution: %d x %d, Resolution: %d", tmp->width, tmp->height, tmp->rate);
        lp__log_trace("Pixel Format: %d, Display Group: %d", tmp->format, tmp->dgid);
        lp__log_trace("Group Offset:  %d, %d", tmp->x_offset, tmp->y_offset);
        lp__log_trace("---------------------------------------------");
    }

    if (lpInitHost(ctx, displays, true) < 0)
    {
        lp__log_error("Unable to initialize lgmp host");
        ret = -1;
        goto destroy_ctx;
    }

    if ((ret = trfSendClientReq(ctx->lp_client.client_ctx, displays)) < 0)
    {
        lp__log_error("Unable to send display request");
        return -1;
    }

    // Create a new subchannel for mouse cursor
    lp__log_trace("Creating subchannel");
    ctx->lp_client.sub_channel = NULL;
    ret = trfCreateSubchannel(ctx->lp_client.client_ctx, 
            &ctx->lp_client.sub_channel, 10);
    if (ret < 0)
    {
        lp__log_error("Unable to create subchannel");
        goto destroy_ctx;
    }

    // Create new thread for cursor
    ret = pthread_create(&ctx->lp_client.cursor_thread, NULL, lpCursorThread ,ctx);
    if (ret < 0)
    {
        lp__log_error("Unable to create thread for cursor");
        goto destroy_ctx;
    }

    ctx->lp_client.sub_started = 1;
    lp__log_trace("Subchannel has been created");
    

    // Set Polling Interval
    ctx->lp_client.client_ctx->opts->fab_poll_rate = ctx->opts.poll_int;
    ctx->lp_client.sub_channel->opts->fab_poll_rate = ctx->opts.poll_int;

    #define timespecdiff(_start, _end) \
    (((_end).tv_sec - (_start).tv_sec) * 1000000000 + \
    ((_end).tv_nsec - (_start).tv_nsec))

    lp__log_trace( "\"Frame\",\"Size\",\"Request (ms)\",\"Frame Time (ms)\""
            ",\"Speed (Gbit/s)\",\"Framerate (Hz)\"\n");
    struct timespec tstart, tend;
    int ctr     = 0;
    int retries = 0;

    displays->mem.ptr = ctx->ram;
    if (ctx->dma_buf)
    {
        lp__log_warn("DMABUF support in LibTRF and LGProxy is highly unstable!");
        lp__log_warn("Use of /dev/shm regions is recommended at this time.");
    }
    ctx->mem_state = LP_MEM_REGISTERED_PERM;
    lp__log_info("Registering memory buffer");
    ret = trfRegDisplayCustom(ctx->lp_client.client_ctx, displays, 
                    ctx->ram_size, 0, FI_WRITE | FI_REMOTE_WRITE);
    if (ret < 0)
    {
        lp__log_error("Unable to register SHM buffer: %s",
                    fi_strerror(abs(ret)));
        return -1;
    }


    while (1)
    {
        if (flag)
            goto destroy_ctx;

        if (ctx->state == LP_STATE_STOP)
        {
            lp__log_info("Shutting down");
            goto destroy_ctx;
        }

        if (ctx->lp_client.thread_flags == T_STOP)
        {
            ret = lpReinitCursorThread(ctx);
            if (ret < 0)
            {
                lp__log_error("Unable to reinitialize cursor thread: %s", strerror(ret));
                goto destroy_ctx;
            }
            lp__log_debug("Cursor thread Reinitialized");
        }
        LGMP_STATUS status;
        status = lgmpHostProcess(ctx->lp_client.lgmp_host);
        if (status != LGMP_OK && status != LGMP_ERR_QUEUE_EMPTY)
        {
            lp__log_error("lgmpHostProcess failed: %s", lgmpStatusString(status));
            return -1;
        }

        if (!lgmpHostQueueHasSubs(ctx->lp_client.host_q))
        {
            retries > 100 ? trfSleep(100) : trfSleep(1);   
            retries++;
            ctr++;
            if (ctr % 10 == 0)
            {
                ret = trfSendKeepAlive(ctx->lp_client.client_ctx);
                if (ret < 0)
                {
                    lp__log_error("Connection error");
                    if (ret == -ETIMEDOUT || ret == -EPIPE)
                        ctx->lp_client.client_ctx->disconnected = 1;
                        
                    goto destroy_ctx;
                }
                lp__log_trace("Sending Keep Alive");
            }
            continue;
        }

        retries = 0;
        displays->mem.ptr = ctx->ram;

        // Update the offset where LGMP has stored the actual framebuffer data
        ret = lpRequestFrame(ctx, displays);
        if (ret < 0)
        {
            lp__log_error("Unable to request frame: %d", ret);
            ret = -1;
            goto destroy_ctx;
        }
        
        clock_gettime(CLOCK_MONOTONIC, &tstart);
        ret = trfRecvFrame(ctx->lp_client.client_ctx, displays);
        if (ret < 0)
        {
            lp__log_error("Unable to receive frame: error %s\n", strerror(-ret));
            return -1;
        }
        clock_gettime(CLOCK_MONOTONIC, &tend);
        double tsd1 = timespecdiff(tstart, tend) / 1000000.0;

        bool repeatframe = false;
        while (1)
        {
            if (flag)
                goto destroy_ctx;

            if (ctx->lp_client.thread_flags == T_ERR)
            {
                goto destroy_ctx;
            }

            if (repeatframe)
            {
                status = lgmpHostQueuePost(
                    ctx->lp_client.host_q, 0,
                    ctx->lp_client.frame_memory[ctx->lp_client.frame_index]);
                if (status != LGMP_OK && status != LGMP_ERR_QUEUE_FULL)
                {
                    lp__log_error("Failed lgmpHostQueuePost: %s", 
                        lgmpStatusString(status));
                    goto destroy_ctx;
                }
                ret = lpSignalFrameDone(ctx, displays);
                if (ret < 0)
                {
                    lp__log_error("Unable to signal frame done: %s", 
                            strerror(ret));
                }
            }

            status = lpKeepLGMPSessionAlive(ctx, displays);
            if (status != LGMP_OK)
            {
                return -1;
            }
        
            ret = lpPollMsg(ctx, &msg);
            if (ret == -EAGAIN)
            {
                trfSleep(ctx->lp_client.client_ctx->opts->fab_poll_rate);
                repeatframe = true;
                continue;
            }
            if (ret < 0)
            {
                lp__log_error("Unable to poll CQ: %s", fi_strerror(-ret));
                return -1;
            }

            uint64_t ifmt = trfPBToInternal(msg->wdata_case);
            if (ifmt == TRFM_KEEP_ALIVE)
            {
                lp__log_debug("Waiting for new data...");
                trf__ProtoFree(msg);
                struct TRFMem msgmem = \
                        ctx->lp_client.client_ctx->xfer.fabric->msg_mem;

                ret = fi_recv(ctx->lp_client.client_ctx->xfer.fabric->ep,
                    trfMemPtr(&msgmem),
                    ctx->lp_client.client_ctx->opts->fab_rcv_bufsize,
                    trfMemFabricDesc(&msgmem), 
                    ctx->lp_client.client_ctx->xfer.fabric->peer_addr, NULL);
                if (ret < 0)
                {
                    lp__log_error("Unable to receive message");
                }
            }
            else if (ifmt == TRFM_SERVER_ACK_F_REQ)
            {
                lp__log_debug("Acknowledgement received...");
                ret = lpSignalFrameDone(ctx, displays);
                if (ret < 0)
                {
                    lp__log_error("Could not signal frame done: %s",
                                  strerror(-ret));
                    goto destroy_ctx;
                }
                repeatframe = false;
                trf__ProtoFree(msg);
                break;
            }
            else
            {
                lp__log_debug("Invalid message type %d", ifmt);
                break;
            }
        }

        clock_gettime(CLOCK_MONOTONIC, &tend);
        double tsd2 = timespecdiff(tstart, tend) / 1000000.0;

        lp__log_trace("%ld,%f,%f,%f,%f",
               trfGetDisplayBytes(displays), tsd1, tsd2, 
               ((double) trfGetDisplayBytes(displays)) / tsd2 / 1e5,
               1000.0 / tsd2);
        displays->frame_cntr++;
    }


destroy_ctx:
    ctx->state = LP_STATE_STOP; // Shutdown all threads

    void * tret = NULL;
    if (ctx->lp_client.sub_started)
    {
        pthread_join(ctx->lp_client.cursor_thread, (void*) &tret);
        if ((uint64_t) tret)
        {
            if ((uint64_t) tret == 1) // Server Requested Disconnect
            {
                lp__log_info("Server requested disconnect");
                ctx->lp_client.client_ctx->disconnected = 1;
            }
            else
            {
                lp__log_error("Thread exited unsuccessfully: %s", strerror((uint64_t) tret));
            }
        }
    }
    lpDestroyContext(ctx);
    if (msg)
    {
        trf__ProtoFree(msg);
    }
    return ret;
}

