#include "lp_sink.h"

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

    pthread_t sub_channel;
    bool sub_started = 0;

    TrfMsg__MessageWrapper * msg = NULL;

    if (lpSetDefaultOpts(ctx))
    {
        lp__log_error("Unable to set default options");
        return -1;
    }
    
    int o;
    while ((o = getopt(argc, argv, "h:p:f:s:d:r")) != -1)
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
                if (ctx->ram_size < 0)
                {
                    lp__log_error("Invalid shared file size passed");
                    return EINVAL;
                }
                break;
            case 'd':
                ctx->opts.delete_exit = true;
                break;
            case 'r':
                ctx->opts.poll_int = atoi(optarg);
                break;
            default:
            case '?':
                lp__log_fatal("Invalid argument -%c", optopt);
                fputs(LP_USAGE_GUIDE_STR, stdout);
                return EINVAL;
        }
    }


    if (!host || !port || !ctx->shm || ctx->ram_size == 0)
    {
        fputs(LP_USAGE_GUIDE_STR, stdout);
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

    PTRFDisplay displays;
    ret = trfGetServerDisplays(ctx->lp_client.client_ctx, &displays);
    if (ret < 0)
    {
        lp__log_error("Unable to get servers display list");
        return -1;
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

    if (lpInitHost(ctx, displays) < 0)
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

    displays->mem.ptr = ctx->ram;
    ret = trfRegDisplayCustom(ctx->lp_client.client_ctx, displays, 
                              ctx->ram_size, 0, FI_WRITE | FI_REMOTE_WRITE);
    if (ret < 0)
    {
        lp__log_error("Unable to register SHM buffer: %s",
                      fi_strerror(abs(ret)));
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

    // Create new thread to use
    ret = pthread_create(&sub_channel, NULL, lpCursorThread ,ctx);
    if (ret < 0)
    {
        lp__log_error("Unable to create thread for cursor");
        goto destroy_ctx;
    }

    sub_started = 1;
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

    while (1)
    {
        if (flag)
            goto destroy_ctx;

        if (ctx->lp_client.thread_flags == T_ERR)
        {
            goto destroy_ctx;
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

        while (1)
        {
            if (flag)
                goto destroy_ctx;

            if (ctx->lp_client.thread_flags == T_ERR)
            {
                goto destroy_ctx;
            }

            status = lpKeepLGMPSessionAlive(ctx);
            if (status != LGMP_OK)
            {
                return -1;
            }
        
            ret = lpPollMsg(ctx, &msg);
            if (ret == -EAGAIN)
            {
                trfSleep(ctx->lp_client.client_ctx->opts->fab_poll_rate);
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
    ctx->lp_client.thread_flags = T_STOPPED;
    uint64_t *tret;
    if (sub_started)
    {
        pthread_join(sub_channel, (void*) &tret);
        if (tret)
        {
            lp__log_error("Thread exited unsuccessfully");
        }
    }
    lpDestroyContext(ctx);
    if (msg)
    {
        trf__ProtoFree(msg);
    }
    return ret;
}

void * lpCursorThread(void * arg)
{
    lp__log_trace("Started Cursor thread");
    PLPContext ctx = (PLPContext) arg;
    intptr_t ret = 0;
    size_t psize = trf__GetPageSize();
    ctx->lp_client.thread_flags = T_RUNNING;
    void * mem = trfAllocAligned(MAX_POINTER_SIZE, psize);
    if (!mem)
    {
        lp__log_error("Unable to allocate memory");
        ret = -ENOMEM;
        goto destroy_ctx;
    }

    KVMFRCursor * cursor = malloc(MAX_POINTER_SIZE);
    if (!cursor)
    {
        lp__log_error("Unable to allocate memory");
        goto destroy_ctx;
    }
    LpMsg__MessageWrapper * wrapper = NULL;

    ret = trfRegInternalMsgBuf(ctx->lp_client.sub_channel, mem, MAX_POINTER_SIZE);
    if (ret < 0)
    {
        lp__log_error("Unable to register internal buffer");
        goto destroy_ctx;
    }

    struct TRFMem *mr = &ctx->lp_client.sub_channel->xfer.fabric->msg_mem;

    while (1)
    {
        if (ctx->lp_client.thread_flags == T_STOPPED) // Parent request to stop thread
        {
            break;
        }

        ret = trfFabricRecv(ctx->lp_client.sub_channel, mr, trfMemPtr(mr),
            MAX_POINTER_SIZE, 
            ctx->lp_client.sub_channel->xfer.fabric->peer_addr,
            ctx->lp_client.sub_channel->opts);
        if (ret < 0)
        {
            lp__log_error("Unable to receive cursor data: %s", fi_strerror(-ret));
            goto destroy_ctx;
        }

        int s = trfMsgGetPackedLength(trfMemPtr(mr));
        lp__log_trace("Packed Length: %d", s);
        ret = trfMsgUnpackProtobuf((ProtobufCMessage **) &wrapper, 
                                   (const ProtobufCMessageDescriptor *) 
                                   &lp_msg__message_wrapper__descriptor, s, 
                                   trfMsgGetPayload(trfMemPtr(mr)));
        if (ret < 0)
        {
            lp__log_error("Unable to decode message");
            ctx->lp_client.thread_flags = T_ERR;
            goto destroy_ctx;
        }

        if (wrapper->wdata_case == LP_MSG__MESSAGE_WRAPPER__WDATA_KA)
        {
            lp__log_debug("Waiting for new data...");
            lp_msg__message_wrapper__free_unpacked(wrapper, NULL);
            wrapper = NULL;
            continue;
        }
        else if (wrapper->wdata_case == 
                LP_MSG__MESSAGE_WRAPPER__WDATA_CURSOR_DATA)
        {
            cursor->y       = wrapper->cursor_data->y;
            cursor->x       = wrapper->cursor_data->x;
            cursor->width   = wrapper->cursor_data->width;
            cursor->height  = wrapper->cursor_data->height;
            cursor->hx      = wrapper->cursor_data->hpx;
            cursor->hy      = wrapper->cursor_data->hpy;
            cursor->type    = wrapper->cursor_data->tex_fmt;
            cursor->pitch   = wrapper->cursor_data->pitch;

            uint32_t flags = wrapper->cursor_data->flags;

            if (wrapper->cursor_data->data.len)
            {
                memcpy((uint8_t *)(cursor + 1), 
                    wrapper->cursor_data->data.data, 
                    wrapper->cursor_data->data.len);

                flags |= CURSOR_FLAG_SHAPE;
                lp__log_trace("Data: %lu bytes", wrapper->cursor_data->data.len);
                
            }
            else
            {
                flags &= ~CURSOR_FLAG_SHAPE;   
            }

            lp_msg__message_wrapper__free_unpacked(wrapper, NULL);

            ret = lpUpdateCursorPos(ctx,cursor, wrapper->cursor_data->data.len, 
                                    flags);
            if (ret == -EAGAIN)
            {
                continue;
            }
            else if (ret < 0)
            {
                ctx->lp_client.thread_flags = T_ERR;
                lp__log_error("Unable to send cursor position to Looking Glass");
                goto destroy_ctx;
            }
            wrapper = NULL;
        }
        else
        {
            lp__log_error("Server sent garbage data");
            lp_msg__message_wrapper__free_unpacked(wrapper, NULL);
            wrapper = NULL;
            ctx->lp_client.thread_flags = T_ERR;
            goto destroy_ctx;
        }
    }

destroy_ctx:
    if (cursor)
    {
        free(cursor);
    }
    trfDestroyContext(ctx->lp_client.sub_channel);
    lp__log_error("Thread exited");
    if (ctx->lp_client.thread_flags != T_ERR)
        ctx->lp_client.thread_flags = T_STOPPED;
    return (void *) ret;
}