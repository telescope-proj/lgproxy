#include "lp_sink.h"


#define USAGEGUIDE "LGProxy Sink Usage Guide\n \
-h\tHost to connect to\n \
-p\tPort to connect to on the host for the NCP Channel\n \
-f\tSHM file to write data into\n \
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

    char* loglevel = getenv("LP_LOG_LEVEL");
    if (!loglevel)
    {
        lp__log_set_level(LP__LOG_FATAL);
        trf__log_set_level(TRF__LOG_FATAL);
    }
    else
    {
        int temp = atoi(loglevel);
        switch (temp)
        {
        case 1:
            lp__log_set_level(LP__LOG_TRACE);
            trf__log_set_level(TRF__LOG_TRACE);
            break;
        case 2:
            lp__log_set_level(LP__LOG_DEBUG);
            trf__log_set_level(TRF__LOG_TRACE);
            break;
        case 3:
            lp__log_set_level(LP__LOG_INFO);
            trf__log_set_level(TRF__LOG_INFO);
            break;
        case 4:
            lp__log_set_level(LP__LOG_WARN);
            trf__log_set_level(TRF__LOG_WARN);
            break;
        case 5:
            lp__log_set_level(LP__LOG_ERROR);
            trf__log_set_level(TRF__LOG_ERROR);
            break;
        default:
            lp__log_set_level(LP__LOG_FATAL);
            trf__log_set_level(TRF__LOG_FATAL);
            break;
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

    displays->mem.ptr = ctx->ram;
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
    int ctr     = 0;
    int retries = 0;

    while (1)
    {
        if (flag)
            goto destroy_ctx;

        LGMP_STATUS status;
        status = lgmpHostProcess(ctx->lp_host.lgmp_host);
        if (status != LGMP_OK && status != LGMP_ERR_QUEUE_EMPTY)
        {
            lp__log_error("lgmpHostProcess failed: %s", lgmpStatusString(status));
            return -1;
        }

        if (!lgmpHostQueueHasSubs(ctx->lp_host.host_q))
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
            if (flag)
                goto destroy_ctx;

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