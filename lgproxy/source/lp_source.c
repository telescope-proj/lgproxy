#include "lp_source.h"

int main(int argc, char ** argv)
{
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
    // if (!metadata)
    // {
    //     lp__log_error("Unable to allocate memory");
    //     ret = -ENOMEM;
    //     goto destroy_ctx;
    // }


    FrameBuffer * fb = NULL;

    // Get the first frame from the host so we have metadata

    if ((ret = lpGetFrame(ctx, &metadata, &fb)) < 0)
    {
        lp__log_error("unable to get framedata");
        ret = -1;
        goto destroy_ctx;
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

    ret = trfUpdateDisplayAddr(ctx->lp_client.client_ctx, displays,
                               framebuffer_get_data(fb));
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

    ssize_t dispBytes = trfGetDisplayBytes(displays);
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
        ret = trfGetMessageAuto(ctx->lp_client.client_ctx, ~TRFM_CLIENT_F_REQ, &processed, 
            (void **) &msg);
        if (ret < 0)
        {
            printf("unable to get poll messages: %d\n", ret);
            return -1;
        }
        if (processed == TRFM_CLIENT_F_REQ)
        {
            // Check if address of fb has changed
            if (framebuffer_get_data(fb) != displays->fb_addr)
            {
                ret = trfUpdateDisplayAddr(ctx->lp_client.client_ctx, displays,
                               framebuffer_get_data(fb));
                if (ret < 0)
                {
                    lp__log_error("Unable to register framebuffer memory for RDMA: %s",
                                fi_strerror((int) abs(ret)));
                    return -1;
                }
            }

            // Get new frame from looking glass
            if ((ret = lpGetFrame(ctx, &metadata, &fb)) < 0)
            {
                lp__log_error("unable to get framedata");
                ret = -1;
                goto destroy_ctx;
            }
            
            // Handle the frame request
            ret = trfSendFrame(ctx->lp_client.client_ctx, displays, msg->client_f_req->addr, 
                msg->client_f_req->rkey);
            if (ret < 0)
            {
                printf("unable to send frame: %d\n", ret);
                return -1;
            }

            struct fi_cq_data_entry de;
            struct fi_cq_err_entry err;

            ret = trfGetSendProgress(ctx->lp_client.client_ctx, &de, &err, 1);
            if (ret <= 0)
            {
                printf("Error: %s\n", fi_strerror(-ret));
                break;
            }
            req_disp->frame_cntr++;
            if((ret = trfAckFrameReq(ctx->lp_client.client_ctx, req_disp)) < 0){
                printf("Unable to send Ack: %s\n", fi_strerror(ret));
            }
            printf("Sent frame: %d\n", req_disp->frame_cntr);
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