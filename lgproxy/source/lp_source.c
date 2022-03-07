#include "lp_source.h"

int main(int argc, char ** argv)
{

    // lp__log_set_level(LP__LOG_ERROR);
    // trf__log_set_level(TRF__LOG_ERROR);

    // PTRFContext ctx = trfAllocContext();
    PLPContext ctx = lpAllocContext();
    if (!ctx)
        return ENOMEM;

    char * host = NULL;
    char * port = NULL;

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
            case '?':
                lp__log_fatal("Invalid argument -%c", optopt);
                return EINVAL;
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
    // Get the first frame from the host so we have metadata

    while (1)
    {
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
        ret = trfGetMessageAuto(ctx->lp_client.client_ctx, TRFM_SET_DISP, 
                &processed, (void **) &msg);

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
    
    ret = trfGetMessageAuto(ctx->lp_client.client_ctx, 0, &processed, (void **) &msg);
    if (ret < 0)
    {
        printf("unable to get poll messages: %d\n", ret);
        return -1;
    }

    if (msg && trfPBToInternal(msg->wdata_case) != TRFM_CLIENT_REQ)
    {
        printf("Wrong Message Type 2: %" PRIu64 "\n", trfPBToInternal(msg->wdata_case));
        return -1;
    }

    // Get the display requested

    PTRFDisplay req_disp = trfGetDisplayByID(displays, 
        msg->client_req->display[0]->id);
    if (!req_disp)
    {
        printf("unable to get display: %s\n", strerror(errno));
        return -1;
    }

    req_disp->fb_addr = ctx->ram;
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
        lp__log_error("Unable to get frame size");
        return -1;
    }

    if (!framebuffer_wait(fb, dispBytes))
    {
        lp__log_error("Wait timedout");
        return -1;
    }


    while (1)
    {
        ret = trfGetMessageAuto(ctx->lp_client.client_ctx, 
                    ~(TRFM_CLIENT_F_REQ | TRFM_KEEP_ALIVE), &processed, 
                    (void **) &msg);
        if (ret < 0)
        {
            printf("unable to get poll messages: %d\n", ret);
            return -1;
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
            if (framebuffer_get_data(fb) != req_disp->fb_addr)
            {
                req_disp->fb_offset = framebuffer_get_data(fb) 
                                      - req_disp->fb_addr;
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
                        lp__log_error("lgmpClientMessageDone: %s", lgmpStatusString(ret));
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
                return -1;
            }

            struct fi_cq_data_entry de;
            struct fi_cq_err_entry err = {0};
            ret = trfGetSendProgress(ctx->lp_client.client_ctx, &de, &err, 1);
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
            break;
        }
        else
        {
            printf("Wrong message type...\n");
        }
    }

    return 0;


destroy_ctx:
    lpDestroyContext(ctx);
    return ret;
}