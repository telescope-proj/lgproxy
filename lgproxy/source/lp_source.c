#include "lp_source.h"


#define USAGEGUIDE "LGProxy Source Usage Guide\n \
-h\tAddress to allow connections from (set to '0.0.0.0' to allow from all)\n \
-p\tPort to listen to for incoming NCP connections\n \
-f\tSHM file to read data from Looking Glass Host\n \
-s\tSize to allocate for SHM File in Bytes\n\n"


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
    pthread_t sub_channel;
    bool sub_started = 0;

    int o;
    while ((o = getopt(argc, argv, "h:p:f:s:")) != -1)
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
                ctx->ram_size = (uint32_t) atoi(optarg);
                break;
            default:
            case '?':
                lp__log_fatal("Invalid argument -%c", optopt);
                printf(USAGEGUIDE);
                return EINVAL;
        }
    }


    if (!host || !port || !ctx->shm || ctx->ram_size == 0)
    {
        printf(USAGEGUIDE);
        return EINVAL;
    }

    char* lpLogLevel = getenv("LP_LOG_LEVEL");
    if (!lpLogLevel)
    {
        lp__log_set_level(LP__LOG_FATAL);
        trf__log_set_level(TRF__LOG_FATAL);
    }
    else
    {
        int temp = atoi(lpLogLevel);
        switch (temp)
        {
        case 1:
            lp__log_set_level(LP__LOG_TRACE);
            break;
        case 2:
            lp__log_set_level(LP__LOG_DEBUG);
            break;
        case 3:
            lp__log_set_level(LP__LOG_INFO);
            break;
        case 4:
            lp__log_set_level(LP__LOG_WARN);
            break;
        case 5:
            lp__log_set_level(LP__LOG_ERROR);
            break;
        default:
            lp__log_set_level(LP__LOG_FATAL);
            break;
        }
    }

    char* trfLogLevel = getenv("TRF_LOG_LEVEL");
    if (!trfLogLevel)
    {
        trf__log_set_level(TRF__LOG_FATAL);
    }
        else
    {
        int temp = atoi(trfLogLevel);
        switch (temp)
        {
        case 1:
            trf__log_set_level(TRF__LOG_TRACE);
            break;
        case 2:
            trf__log_set_level(TRF__LOG_DEBUG);
            break;
        case 3:
            trf__log_set_level(TRF__LOG_INFO);
            break;
        case 4:
            trf__log_set_level(TRF__LOG_WARN);
            break;
        case 5:
            trf__log_set_level(TRF__LOG_ERROR);
            break;
        default:
            trf__log_set_level(TRF__LOG_FATAL);
            break;
        }
    }


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

    if((ret = trfNCAccept(ctx->lp_host.server_ctx, &ctx->lp_client.client_ctx)) < 0)
    {
        lp__log_error("Unable to Accept Client Connection");
        ret = -1;
        goto destroy_ctx;
    }

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
    LGMP_STATUS status;
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
    
    ret = trfBindDisplayList(ctx->lp_client.client_ctx, displays); //Bind display list to client context
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
        
        ret = trfGetMessageAuto(ctx->lp_client.client_ctx, TRFM_SET_DISP, 
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
    
    ret = trfGetMessageAuto(ctx->lp_client.client_ctx, 0, &processed, (void **) 
                &msg, &opaque);
    if (ret < 0)
    {
        printf("unable to get poll messages: %d\n", ret);
        ret = 0;
        goto destroy_ctx;
    }

    if (msg && trfPBToInternal(msg->wdata_case) != TRFM_CLIENT_REQ)
    {
        printf("Wrong Message Type 2: %" PRIu64 "\n", trfPBToInternal(msg->wdata_case));
        ret = 0;
        goto destroy_ctx;
    }

    // Get the display requested

    PTRFDisplay req_disp = trfGetDisplayByID(displays, 
        msg->client_req->display[0]->id);
    if (!req_disp)
    {
        printf("unable to get display: %s\n", strerror(errno));
        ret = 0;
        goto destroy_ctx;
    }

    req_disp->mem.ptr = ctx->ram;
    // req_disp->fb_addr = ctx->ram;
    ret = trfRegDisplayCustom(ctx->lp_client.client_ctx, req_disp, 
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
    ret = trfAckClientReq(ctx->lp_client.client_ctx, &disp_id, 1);
    if (ret < 0)
    {
        printf("unable to acknowledge request: %d\n", ret);
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

    while (1)
    {
        if (flag)
        {
            lp__log_error("Destroying CTX");
            goto destroy_ctx;
        }

        if (ctx->lp_client.thread_flags == T_ERR)
        {
            uint64_t *tret;
            pthread_join(sub_channel, (void*) &tret);
            lp__log_error("Subchannel has exited with an error: %s", 
                fi_strerror(abs((uint64_t) tret)));
            goto destroy_ctx;
        }

        ret = trfGetMessageAuto(ctx->lp_client.client_ctx, 
                    ~(TRFM_CLIENT_F_REQ | TRFM_KEEP_ALIVE | TRFM_CH_OPEN), &processed, 
                    (void **) &msg, &opaque);
        if (ret < 0)
        {
            printf("unable to get poll messages: %d\n", ret);
            ret = -1;
            goto destroy_ctx;
        }
        if (processed == TRFM_CH_OPEN)
        {
            ctx->lp_client.sub_channel = NULL;
            ret = trfProcessSubchannelReq(ctx->lp_client.client_ctx, 
                &ctx->lp_client.sub_channel, msg);
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

                if (ctx->lp_client.thread_flags == T_ERR)
                {
                    uint64_t *tret;
                    pthread_join(sub_channel, (void*) &tret);
                    lp__log_error("Subchannel has exited with an error: %s", 
                        fi_strerror(abs((uint64_t) tret)));
                    goto destroy_ctx;
                }

                ret = lpGetFrame(ctx, &metadata, &fb);
                if (ret == -EAGAIN)
                {
                    if (trf__HasPassed(CLOCK_MONOTONIC, &te))
                    {
                        lp__log_debug("Sending keep alive...");
                        ret = trfSendKeepAlive(ctx->lp_client.client_ctx);
                        if (ret < 0)
                        {
                            lp__log_debug("Error sending keep alive: %s", fi_strerror(abs(ret)));
                            return ret;
                        }
                        ret = clock_gettime(CLOCK_MONOTONIC, &ts);
                        if (ret < 0)
                        {
                            trf__log_error("System clock error: %s", strerror(errno));
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
                    ret = lgmpClientMessageDone(ctx->lp_client.client_q);
                    if (ret != LGMP_OK)
                    {
                        lp__log_error("lgmpClientMessageDone: %s", 
                            lgmpStatusString(ret));
                    }
                    lp__log_debug("Repeated frame");
                    continue;
                }
                lp__log_debug("Got frame from LG");
                break;
            }
        
            // Handle the frame request
            ret = trfSendFrame(ctx->lp_client.client_ctx, displays, 
                               msg->client_f_req->addr, 
                               msg->client_f_req->rkey);
            if (ret < 0)
            {
                printf("unable to send frame: %d\n", ret);
                ret = -1;
                goto destroy_ctx;
            }

            struct fi_cq_data_entry de;
            struct fi_cq_err_entry err = {0};
            ret = trfGetSendProgress(ctx->lp_client.client_ctx, &de, &err, 1, 
                    ctx->lp_client.client_ctx->opts);
            if (ret <= 0)
            {
                lp__log_error("Error: %s", fi_strerror(err.err));
                break;
            }

            //Post receive message to LGMP
            ret = lgmpClientMessageDone(ctx->lp_client.client_q);
            if (ret != LGMP_OK && ret != LGMP_ERR_QUEUE_EMPTY)
            {
                lp__log_debug("lgmpClientMessageDone: %s", lgmpStatusString(ret));
            }

            req_disp->frame_cntr++;
            ret = trfAckFrameReq(ctx->lp_client.client_ctx, req_disp);
            if (ret < 0)
            {
                printf("Unable to send Ack: %s\n", fi_strerror(ret));
            }
        }
        else if (processed == TRFM_DISCONNECT)
        {
            // If the peer initiates a disconnect, setting this flag will ensure
            // that a disconnect message is not sent back to an already
            // disconnected peer (which results in a wait until the timeout).
            ctx->lp_host.server_ctx->disconnected = 1;
            printf("Client requested a disconnect\n");
            goto destroy_ctx;
            break;
        }
        else
        {
            printf("Wrong message type...\n");
        }
    }

    return 0;


destroy_ctx:
    ctx->lp_client.thread_flags = T_STOPPED;
    uint64_t *tret;
    if (sub_started)
    {
        pthread_join(sub_channel, (void*) &tret);
    }
    if (tret)
    {
        lp__log_error("Thread exited unsuccessfully: %d", tret);
    }
    status = lgmpClientUnsubscribe(&ctx->lp_client.client_q);
    if (status != LGMP_OK)
    {
        lp__log_error("Unable to unsubscribe from host");
    }
    lpDestroyContext(ctx);
    return ret;
}

void * lpHandleCursorPos(void * arg)
{
    lp__log_trace("Subchannel thread started");
    PLPContext ctx = (PLPContext) arg;
    ctx->lp_client.thread_flags = T_RUNNING;
    intptr_t ret = 0;
    uint32_t cursorSize = 0;
    size_t psize = trf__GetPageSize();
    uint32_t flags = 0;
    void * cursorData = trfAllocAligned(MAX_POINTER_SIZE, psize);
    if (!cursorData)
    {
        lp__log_error("Unable to allocate memory for subchannel");
        ctx->lp_client.thread_flags = T_ERR;
        ret = -EINVAL;
        goto destroy_ctx;
    }
    KVMFRCursor *cursor = (KVMFRCursor *) cursorData;
    cursor = NULL;

    ret = trfRegInternalMsgBuf(ctx->lp_client.sub_channel, cursorData,
            MAX_POINTER_SIZE);
    if (ret)
    {
        lp__log_error("Unable to register buffer");
        ctx->lp_client.thread_flags = T_ERR;
        ret = -ret;
        goto destroy_ctx;
    }
    struct TRFMem *mr   = &ctx->lp_client.sub_channel->xfer.fabric->msg_mem;
    void * buf          = trfMemPtr(mr);
    
    LpMsg__MessageWrapper wrapper   = LP_MSG__MESSAGE_WRAPPER__INIT;
    LpMsg__CursorData curData       = LP_MSG__CURSOR_DATA__INIT;
    wrapper.cursor_data             = &curData;
    wrapper.wdata_case              = LP_MSG__MESSAGE_WRAPPER__WDATA_CURSOR_DATA;

    struct timespec te;
    bool setDeadline = false;
    while (1)
    {
        if (ctx->lp_client.thread_flags == T_STOPPED)
        {
            break;
        }
        if (!setDeadline)
        {
            trfGetDeadline(&te, 1000);
            setDeadline = true;
            lp__log_trace("Setting new deadline");
        }

        if ((ret = lpgetCursor(ctx, &cursor, &cursorSize, &flags)) < 0)
        {
            lp__log_error("Unable to get cursor position");
            ctx->lp_client.thread_flags = T_ERR;
            goto destroy_ctx;
        }

        if (trf__HasPassed(CLOCK_MONOTONIC, &te) && !cursor)
        {
            lp__log_debug("Sending cursor keep alive...");
            ret = lpKeepAlive(ctx->lp_client.sub_channel);
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

            if (cursorSize > sizeof(KVMFRCursor))
            {
                curData.data.len = cursorSize - sizeof(KVMFRCursor);
                curData.data.data = (uint8_t *)(cursor + 1);
                    lp__log_trace("Data on Sink");

                uint8_t *tmp = (uint8_t *) (cursor + 1);
                for (int i = 0; i < 50; i++)
                {
                    printf("%hhx", *tmp);
                    tmp++;
                }
                printf("\n");

            }
            else
            {
                curData.data.len = 0;
                curData.data.data = NULL;
            }
            
            lp__log_trace("Mouse x: %d; y: %d; p: %d, s: %lu, len: %lu, data: %p", 
                          cursor->x, cursor->y, cursor->pitch, 
                          cursorSize,
                          curData.data.len, curData.data.data);

            ret = trfMsgPackProtobuf((ProtobufCMessage *) &wrapper, 
                                     MAX_POINTER_SIZE, buf);
            if (ret < 0)
            {
                lp__log_error("Unable to pack message");
                ctx->lp_client.thread_flags = T_ERR;
                goto destroy_ctx;
            }

            free(cursor);

            ret = trfFabricSend(ctx->lp_client.sub_channel, mr, trfMemPtr(mr), ret, 
                    ctx->lp_client.sub_channel->xfer.fabric->peer_addr,
                    ctx->lp_client.sub_channel->opts);
            if (ret < 0)
            {
                lp__log_error("Unable to send cursor data %s", fi_strerror(ret));
                ctx->lp_client.thread_flags = T_ERR;
                goto destroy_ctx;
            }
            cursor = NULL;
            setDeadline = false;
        }
    }
    
    ret = 0;

destroy_ctx:
    trfDestroyContext(ctx->lp_client.sub_channel);
    lp__log_error("Exited Subchannel");
    if (ctx->lp_client.thread_flags != T_ERR)
        ctx->lp_client.thread_flags = T_STOPPED;
    return (void *) ret;
}