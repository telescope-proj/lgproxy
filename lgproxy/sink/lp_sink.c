#include "lp_sink.h"


int main(int argc, char ** argv){
    
    // lp__log_set_level(LP__LOG_ERROR);
    // trf__log_set_level(TRF__LOG_ERROR);
    PLPContext ctx = lpAllocContext();
    if (!ctx) {
        return - EINVAL;
    }
    int ret = 0;
    char * host = NULL;
    char * port = NULL;

    TrfMsg__MessageWrapper * msg = NULL;

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

    if ((ret = lpTrfClientInit(ctx, host, port)) < 0)
    {
        lp__log_error("Unable to initialize trf client");
        return -1;
    }

    PTRFDisplay displays;
    printf("Retrieving displays");
    ret = trfGetServerDisplays(ctx->lp_client.client_ctx, &displays);
    if (ret < 0)
    {
        lp__log_error("Unable to get servers display list");
        return -1;
    }

    printf("Server Display List\n");
    printf("---------------------------------------------\n");
    for (PTRFDisplay tmp = displays; tmp != NULL; tmp = tmp->next)
    {
        printf("Display ID:    %d\n", tmp->id);
        printf("Display Name:  %s\n", tmp->name);
        printf("Resolution:    %d x %d\n", tmp->width, tmp->height);
        printf("Refresh Rate:  %d\n", tmp->rate);
        printf("Pixel Format:  %d\n", tmp->format);
        printf("Display Group: %d\n", tmp->dgid);
        printf("Group Offset:  %d, %d\n", tmp->x_offset, tmp->y_offset);
        printf("---------------------------------------------\n");
    }

    if (lpInitHost(ctx, displays) < 0)
    {
        lp__log_error("Unable to initialize lgmp host");
        ret = -1;
        goto destroy_ctx;
    }

    if ((ret = trfSendClientReq(ctx->lp_client.client_ctx, displays)) < 0)
    {
        printf("Unable to send display request");
        return -1;
    }

    displays->fb_addr = ctx->ram;
    ret = trfRegDisplayCustom(ctx->lp_client.client_ctx, displays, 
                              ctx->ram_size, 0, FI_WRITE | FI_REMOTE_WRITE);
    if (ret < 0)
    {
        lp__log_error("Unable to register SHM buffer: %s",
                      fi_strerror(abs(ret)));
        return -1;
    }

    #define timespecdiff(_start, _end) \
    (((_end).tv_sec - (_start).tv_sec) * 1000000000 + \
    ((_end).tv_nsec - (_start).tv_nsec))

    
    printf( "\"Frame\",\"Size\",\"Request (ms)\",\"Frame Time (ms)\""
            ",\"Speed (Gbit/s)\",\"Framerate (Hz)\"\n");
    struct timespec tstart, tend;
    int ctr = 0;

    while (1)
    {
        LGMP_STATUS status;
        status = lgmpHostProcess(ctx->lp_host.lgmp_host);
        if (status != LGMP_OK && status != LGMP_ERR_QUEUE_EMPTY)
        {
            lp__log_error("lgmpHostProcess failed: %s", lgmpStatusString(status));
            return -1;
        }

        if (!lgmpHostQueueHasSubs(ctx->lp_host.host_q))
        {
            lp__log_debug("Waiting.....");
            trfSleep(100);
            ctr++;
            if (ctr % 10 == 0)
            {
                trfSendKeepAlive(ctx->lp_client.client_ctx);
            }
            continue;
        }

        // Update the offset where LGMP has stored the actual framebuffer data
        ret = lpRequestFrame(ctx, displays);
        if (ret < 0)
        {
            lp__log_error("Unable to request frame: %d", ret);
            ret = -1;
            goto destroy_ctx;
        }
        
        // Post a frame receive request. This will inform the server that the
        // client is ready to receive a frame, but the frame will not be ready
        // until the server sends an acknowledgement.
        clock_gettime(CLOCK_MONOTONIC, &tstart);
        ret = trfRecvFrame(ctx->lp_client.client_ctx, displays);
        if (ret < 0)
        {
            printf("Unable to receive frame: error %s\n", strerror(-ret));
            return -1;
        }
        clock_gettime(CLOCK_MONOTONIC, &tend);
        double tsd1 = timespecdiff(tstart, tend) / 1000000.0;

        while (1)
        {
            
            lp__log_trace("LGMP Status: %s", lgmpStatusString(status));
            struct fi_cq_data_entry de;
            struct fi_cq_err_entry err;
            ret = trfGetRecvProgress(ctx->lp_client.client_ctx, &de, &err, 1);
            if (ret < 0)
            {
                printf("Unable to get receive progress: error %s\n", 
                    strerror(-ret));
                return -1;
            }
            ret = trfMsgUnpack(&msg, 
                    trfMsgGetPackedLength(ctx->lp_client.client_ctx->xfer.fabric->msg_ptr),
                    trfMsgGetPayload(ctx->lp_client.client_ctx->xfer.fabric->msg_ptr));
            if (ret < 0)
            {
                printf("Unable to unpack: %s\n", 
                    strerror(-ret));
                return -1;
            }
            uint64_t ifmt = trfPBToInternal(msg->wdata_case);
            if (ifmt == TRFM_KEEP_ALIVE)
            {
                lp__log_debug("Waiting for new data...");
                trf__ProtoFree(msg);
                ret = fi_recv(ctx->lp_client.client_ctx->xfer.fabric->ep, 
                    ctx->lp_client.client_ctx->xfer.fabric->msg_ptr, 
                    ctx->lp_client.client_ctx->opts->fab_rcv_bufsize,
                    fi_mr_desc(ctx->lp_client.client_ctx->xfer.fabric->msg_mr), 
                    ctx->lp_client.client_ctx->xfer.fabric->peer_addr, NULL);
                if (ret < 0)
                {
                    lp__log_error("Unable to receive message");
                }
            }
            else if (ifmt == TRFM_SERVER_ACK_F_REQ)
            {
                lp__log_debug("Acknowledgement received...");
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

        printf("%ld,%f,%f,%f,%f\n",
               trfGetDisplayBytes(displays), tsd1, tsd2, 
               ((double) trfGetDisplayBytes(displays)) / tsd2 / 1e5,
               1000.0 / tsd2);
        displays->frame_cntr++;
    }

    

destroy_ctx:
    lpDestroyContext(ctx);
    return ret;
}