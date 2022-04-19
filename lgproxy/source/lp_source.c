#include "lp_source.h"


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
"   -h  Hostname or IP address to listen on\n"                          \
"   -p  Port or service name to listen on\n"                            \
"   -f  Shared memory or KVMFR file to use\n"                           \
"   -s  Size of the shared memory file\n";


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
    if (!ctx)
        return ENOMEM;

    char * host = NULL;
    char * port = NULL;

    if (lpSetDefaultOpts(ctx))
    {
        lp__log_error("Unable to set default options");
        return -1;
    }

    int o;
    while ((o = getopt(argc, argv, "h:p:f:s:r:")) != -1)
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
            case 'r':
                lp__log_trace("Poll Interval: %s", optarg);
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
        return EINVAL;
    }

    lpSetLPLogLevel();  // Set lgproxy log level
    lpSetTRFLogLevel(); // Set libtrf log level


    if (!host || !port)
    {
        lp__log_fatal("No host or port specified");
        return EINVAL;
    }

    int ret;

    if ((ret = lpTrfServerInit(ctx, host, port)) < 0)
    {
        lp__log_fatal("Unable to initialize libtrf server");
        ret = -1;
        goto destroy_ctx;
    }

    while(1)
    {
        lp__log_info("Waiting for Client Connection ...");
        if((ret = trfNCAccept(ctx->lp_host.server_ctx, &ctx->lp_host.client_ctx)) < 0)
        {
            lp__log_error("Unable to Accept Client Connection");
            ret = -1;
            goto destroy_ctx;
        }
        lp__log_info("New Client Connected");
        if ((ret = lpHandleClientReq(ctx)) < 0)
        {
            lp__log_error("Client disconnection with error: %s", fi_strerror(ret));
        }
    }

destroy_ctx:
    lpDestroyContext(ctx);
    return ret;

}

int lpHandleClientReq(PLPContext ctx)
{
    pthread_t sub_channel;
    bool sub_started = 0;
    int ret = 0;
    lp__log_trace("Accepted Connection");

    struct stat fileStat;
    if (stat(ctx->shm, &fileStat) != 0)
    {
        lp__log_error("Cannot stat SHM file: %s", ctx->shm);
        lp__log_error("Does it exist and have you the appropriate permissions been set?");
        goto destroy_ctx;
    }
    
    ctx->ram_size = fileStat.st_size;
    lp__log_trace("SHM File %s opened, size: %lu", ctx->shm, ctx->ram_size);

    if ((ret = lpInitLgmpClient(ctx) < 0)) // Initialize LGMP Client 
    {
        lp__log_fatal("Unable to initialize the lgmp client: %s", 
                    strerror(ret));
        ret = -1;
        goto destroy_ctx;
    }

    if ((ret = lpClientInitSession(ctx)) < 0) // Initialize a connection with LGMP host
    {
        lp__log_fatal("Unable to initialize lgmp session: %s", strerror(ret));
        ret = -1;
        goto destroy_ctx;
    }

    PTRFDisplay displays = calloc(1, sizeof(struct TRFDisplay));
    if (!displays)
    {
        trf__log_trace("unable to allocate memory for display data");
        ret = -ENOMEM;
        goto destroy_ctx;
    }

    KVMFRFrame * metadata = NULL; 
    FrameBuffer * fb = NULL;
    int opaque = 0;

    // Get the first frame from the host so we have metadata

    while (1)
    {
        if (flag)
            goto destroy_ctx;
        
        ret = lpGetFrame(ctx, &metadata, &fb);
        if ((ret == -EAGAIN))
        {
            continue;
        }
        else if (ret < 0 && (ret != -EAGAIN))
        {
            lp__log_error("unable to get framedata");
            ret = -1;
            goto destroy_ctx;
        }
        break;
    }
    displays->id        =   0;
    displays->name      =   "Looking Glass Display";
    displays->height    =   metadata->realHeight ? \
                            metadata->realHeight : metadata->height;
    displays->width     =   metadata->width;
    displays->format    =  lpLGToTrfFormat(metadata->type);
    displays->rate      =   0;
    
    ret = trfBindDisplayList(ctx->lp_host.client_ctx, displays); //Bind display list to client context
    if (ret < 0)
    {
        trf__log_error("Unable to bind display list");
        ret = -1;
        goto destroy_ctx;
    }

    uint64_t processed;
    TrfMsg__MessageWrapper *msg = NULL;
    while (1){
        if (flag)
            goto destroy_ctx;
        
        ret = trfGetMessageAuto(ctx->lp_host.client_ctx, TRFM_SET_DISP, 
                &processed, (void **) &msg, &opaque);

        if (ret > 0)
        {
            printf("unable to get poll messages");
            continue;
        }
        break;
    }

    if (msg && trfPBToInternal(msg->wdata_case) != TRFM_CLIENT_DISP_REQ)
    {
        lp__log_error("Wrong message type 1: %s " PRIu64 "\n", 
                    trfPBToInternal(msg->wdata_case));
    }

    lp__log_trace("Client requested display");
    
    ret = trfGetMessageAuto(ctx->lp_host.client_ctx, 0, &processed, (void **) 
                &msg, &opaque);
    if (ret < 0)
    {
        lp__log_error("unable to get poll messages: %d\n", ret);
        ret = 0;
        goto destroy_ctx;
    }

    if (msg && trfPBToInternal(msg->wdata_case) != TRFM_CLIENT_REQ)
    {
        lp__log_error("Wrong Message Type 2: %" PRIu64 "\n", trfPBToInternal(msg->wdata_case));
        ret = 0;
        goto destroy_ctx;
    }

    // Get the display requested

    PTRFDisplay req_disp = trfGetDisplayByID(displays, 
        msg->client_req->display[0]->id);
    if (!req_disp)
    {
        lp__log_error("unable to get display: %s\n", strerror(errno));
        ret = 0;
        goto destroy_ctx;
    }

    req_disp->mem.ptr = ctx->ram;

    ret = trfRegDisplayCustom(ctx->lp_host.client_ctx, req_disp, 
                              ctx->ram_size, 
                              framebuffer_get_data(fb) - (uint8_t *) ctx->ram, 
                              FI_READ);
    if (ret < 0)
    {
        lp__log_error("Unable to register framebuffer memory for RDMA: %s",
                      fi_strerror((int) abs(ret)));
        return -1;
    }

    uint32_t disp_id = req_disp->id;
    ret = trfAckClientReq(ctx->lp_host.client_ctx, &disp_id, 1);
    if (ret < 0)
    {
        lp__log_error("unable to acknowledge request: %d\n", ret);
        return -1;
    }

    ssize_t dispBytes = trfGetDisplayBytes(req_disp);
    if (dispBytes < 0)
    {
        lp__log_error("Unable to get frame size: %d", dispBytes);
        return -1;
    }

    if (!framebuffer_wait(fb, dispBytes))
    {
        lp__log_error("Wait timedout");
        return -1;
    }
    // Set Polling Interval
    ctx->lp_host.client_ctx->opts->fab_poll_rate = ctx->opts.poll_int;
    while (1)
    {
        if (flag)
        {
            lp__log_error("Destroying CTX");
            goto destroy_ctx;
        }

        if (ctx->lp_host.thread_flags == T_ERR)
        {
            goto destroy_ctx;
        }

        ret = trfGetMessageAuto(ctx->lp_host.client_ctx, 
                    ~(TRFM_CLIENT_F_REQ | TRFM_KEEP_ALIVE | TRFM_CH_OPEN), &processed, 
                    (void **) &msg, &opaque);
        if (ret < 0)
        {
            lp__log_error("unable to get poll messages: %d\n", ret);
            ret = -1;
            goto destroy_ctx;
        }
        if (processed == TRFM_CH_OPEN)
        {
            ctx->lp_host.sub_channel = NULL;
            ret = trfProcessSubchannelReq(ctx->lp_host.client_ctx, 
                &ctx->lp_host.sub_channel, msg);
            if (ret < 0)
            {
                lp__log_error("Unable to create subchannel");
                ret = -1;
                goto destroy_ctx;
            }
            lp__log_trace("Subchannel Opened");

            // Create thread for subchannel
            ret = pthread_create(&sub_channel, NULL, lpHandleCursorPos, 
                ctx);
            if (ret)
            {
                lp__log_error("Unable to create thread to handle cursor position");
                ret = -1;
                goto destroy_ctx;
            }
            ctx->lp_host.sub_channel->opts->fab_poll_rate = ctx->opts.poll_int;
            sub_started = 1;
        }
        if (processed == TRFM_KEEP_ALIVE)
        {
            lp__log_debug("Received keep alive...");
            trf__ProtoFree(msg);
            continue;
        }
        if (processed == TRFM_CLIENT_F_REQ)
        {
            // Check if address of fb has changed
            if (framebuffer_get_data(fb) != req_disp->mem.ptr)
            {
                req_disp->fb_offset = framebuffer_get_data(fb) 
                                      - (uint8_t *) req_disp->mem.ptr;
            }

            struct timespec ts, te;
            ret = clock_gettime(CLOCK_MONOTONIC, &ts);
            if (ret < 0)
            {
                trf__log_error("System clock error: %s", strerror(errno));
                return -errno;
            }
            trf__GetDelay(&ts, &te, 1000);

            trf__log_debug("Waiting for new frame data...");

            // Get new frame from Looking Glass
            while (1)
            {
                if (flag)
                    goto destroy_ctx;

                if (ctx->lp_host.thread_flags == T_ERR)
                {
                    goto destroy_ctx;
                }

                ret = lpGetFrame(ctx, &metadata, &fb);
                if (ret == -EAGAIN)
                {
                    if (trf__HasPassed(CLOCK_MONOTONIC, &te))
                    {
                        lp__log_debug("Sending keep alive...");
                        if (ctx->lp_host.thread_flags == T_ERR)
                        {
                            goto destroy_ctx;
                        }
                        ret = trfSendKeepAlive(ctx->lp_host.client_ctx);
                        if (ret < 0)
                        {
                            lp__log_debug("Error sending keep alive: %s", 
                                    fi_strerror(abs(ret)));
                            return ret;
                        }
                        ret = clock_gettime(CLOCK_MONOTONIC, &ts);
                        if (ret < 0)
                        {
                            trf__log_error("System clock error: %s", 
                                    strerror(errno));
                            return -errno;
                        }
                        trf__GetDelay(&ts, &te, 1000);
                        lp__log_debug("Sent keep alive");
                    }
                    continue;
                }
                else if (ret < 0 && ret != -EAGAIN)
                {
                    lp__log_error("unable to get framedata: %d", ret);
                    ret = -1;
                    goto destroy_ctx;
                }
                framebuffer_wait(fb, trfGetDisplayBytes(displays));
                if (msg->client_f_req->frame_cntr == metadata->frameSerial)
                {
                    ret = lgmpClientMessageDone(ctx->lp_host.client_q);
                    if (ret != LGMP_OK)
                    {
                        lp__log_error("lgmpClientMessageDone: %s", 
                            lgmpStatusString(ret));
                    }
                    lp__log_debug("Repeated frame");
                    continue;
                }
                lp__log_debug("Got frame from LookingGlass");
                break;
            }
        
            // Handle the frame request
            ret = trfSendFrame(ctx->lp_host.client_ctx, displays, 
                               msg->client_f_req->addr, 
                               msg->client_f_req->rkey);
            if (ret < 0)
            {
                lp__log_error("unable to send frame: %d\n", ret);
                ret = -1;
                goto destroy_ctx;
            }

            struct fi_cq_data_entry de;
            struct fi_cq_err_entry err = {0};
            ret = trfGetSendProgress(ctx->lp_host.client_ctx, &de, &err, 1, 
                    ctx->lp_host.client_ctx->opts);
            if (ret <= 0)
            {
                lp__log_error("Error: %s", fi_strerror(err.err));
                break;
            }

            //Post receive message to LGMP
            ret = lgmpClientMessageDone(ctx->lp_host.client_q);
            if (ret != LGMP_OK && ret != LGMP_ERR_QUEUE_EMPTY)
            {
                lp__log_debug("lgmpClientMessageDone: %s", lgmpStatusString(ret));
            }

            req_disp->frame_cntr++;
            ret = trfAckFrameReq(ctx->lp_host.client_ctx, req_disp);
            if (ret < 0)
            {
                lp__log_error("Unable to send Ack: %s\n", fi_strerror(ret));
            }
        }
        else if (processed == TRFM_DISCONNECT)
        {
            ctx->lp_host.server_ctx->disconnected = 1;
            lp__log_debug("Client requested a disconnect\n");
            goto destroy_ctx;
        }
        else
        {
            lp__log_debug("Wrong message type...\n");
        }
    }

destroy_ctx:
    ctx->lp_host.thread_flags = T_STOPPED;
    uint64_t *tret;
    if (sub_started)
    {
        pthread_join(sub_channel, (void*) &tret);
        if (tret)
        {
            lp__log_error("Thread exited unsuccessfully: %d", tret);
        }
    }
    trfDestroyContext(ctx->lp_host.client_ctx);
    ctx->lp_host.client_ctx = NULL;
    return ret;
}

void * lpHandleCursorPos(void * arg)
{
    lp__log_trace("Subchannel thread started");
    PLPContext ctx = (PLPContext) arg;
    ctx->lp_host.thread_flags = T_RUNNING;
    intptr_t ret = 0;
    uint32_t cursorSize = 0;
    size_t psize = trf__GetPageSize();
    uint32_t flags = 0;
    void * cursorData = trfAllocAligned(MAX_POINTER_SIZE, psize);
    if (!cursorData)
    {
        lp__log_error("Unable to allocate memory for subchannel");
        ctx->lp_host.thread_flags = T_ERR;
        ret = -EINVAL;
        goto destroy_ctx;
    }
    KVMFRCursor *cursor = (KVMFRCursor *) cursorData;
    cursor = NULL;

    ret = trfRegInternalMsgBuf(ctx->lp_host.sub_channel, cursorData,
            MAX_POINTER_SIZE);
    if (ret)
    {
        lp__log_error("Unable to register buffer");
        ctx->lp_host.thread_flags = T_ERR;
        ret = -ret;
        goto destroy_ctx;
    }
    struct TRFMem *mr   = &ctx->lp_host.sub_channel->xfer.fabric->msg_mem;
    void * buf          = trfMemPtr(mr);
    
    LpMsg__MessageWrapper wrapper   = LP_MSG__MESSAGE_WRAPPER__INIT;
    LpMsg__CursorData curData       = LP_MSG__CURSOR_DATA__INIT;
    wrapper.cursor_data             = &curData;
    wrapper.wdata_case              = LP_MSG__MESSAGE_WRAPPER__WDATA_CURSOR_DATA;

    struct timespec te;
    bool setDeadline = false;
    while (1)
    {
        if (ctx->lp_host.thread_flags == T_STOPPED)
        {
            break;
        }
        if (!setDeadline)
        {
            trfGetDeadline(&te, 1000);
            setDeadline = true;
        }

        if ((ret = lpgetCursor(ctx, &cursor, &cursorSize, &flags)) < 0)
        {
            lp__log_error("Unable to get cursor position");
            ctx->lp_host.thread_flags = T_ERR;
            goto destroy_ctx;
        }

        if (trf__HasPassed(CLOCK_MONOTONIC, &te) && !cursor)
        {
            lp__log_debug("Sending cursor keep alive...");
            ret = lpKeepAlive(ctx->lp_host.sub_channel);
            if (ret < 0)
            {
                lp__log_error("Error sending keep alive: %s", fi_strerror(abs(ret)));
                return (void *) ret;
            }
            setDeadline = false;
            lp__log_debug("Sent keep alive");
            continue;
        }
        
        if (cursor)
        {
            curData.y       = cursor->y;
            curData.x       = cursor->x;
            curData.width   = cursor->width;
            curData.height  = cursor->height;
            curData.hpx     = cursor->hx;
            curData.hpy     = cursor->hy;
            curData.tex_fmt = cursor->type;
            curData.pitch   = cursor->pitch;
            curData.flags   = flags;

            if (cursorSize > sizeof(KVMFRCursor)) // Send cursor shape data
            {
                curData.data.len = cursorSize - sizeof(KVMFRCursor);
                curData.data.data = (uint8_t *)(cursor + 1);
            }
            else // Only cursor position has changed
            {
                curData.data.len = 0;
                curData.data.data = NULL;
            }

            ret = trfMsgPackProtobuf((ProtobufCMessage *) &wrapper, 
                                     MAX_POINTER_SIZE, buf);
            if (ret < 0)
            {
                lp__log_error("Unable to pack message");
                ctx->lp_host.thread_flags = T_ERR;
                goto destroy_ctx;
            }

            free(cursor);

            ret = trfFabricSend(ctx->lp_host.sub_channel, mr, trfMemPtr(mr), ret, 
                    ctx->lp_host.sub_channel->xfer.fabric->peer_addr,
                    ctx->lp_host.sub_channel->opts);
            if (ret < 0)
            {
                lp__log_error("Unable to send cursor data %s", fi_strerror(ret));
                ctx->lp_host.thread_flags = T_ERR;
                goto destroy_ctx;
            }
            cursor = NULL;
            setDeadline = false;
        }
    }
    
    ret = 0;

destroy_ctx:
    trfDestroyContext(ctx->lp_host.sub_channel);
    ctx->lp_host.sub_channel = NULL;
    lp__log_debug("Exited Subchannel");
    if (ctx->lp_host.thread_flags != T_ERR)
        ctx->lp_host.thread_flags = T_STOPPED;
    return (void *) ret;
}