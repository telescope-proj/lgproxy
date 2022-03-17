#include "lp_sink.h"


int main(int argc, char ** argv){
    PLPContext ctx = lpAllocContext();
    if (!ctx) {
        return - EINVAL;
    }
    int ret = 0;
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

    if ((ret = trfSendClientReq(ctx->lp_client.client_ctx, displays)) < 0)
    {
        printf("Unable to send display request");
        return -1;
    }

    displays->fb_addr = trfAllocAligned(trfGetDisplayBytes(displays), 2097152);
    if (!displays->fb_addr)
    {
        lp__log_error("Unable to allocate memory for display");
        return -1;

    }

    ret = trfRegDisplaySink(ctx->lp_client.client_ctx, displays);
    if (ret < 0)
    {
        printf("Unable to register display sink: error %s\n", strerror(ret));
        return -1;
    }

    if (lpInitHost(ctx, displays) < 0)
    {
        lp__log_error("Unable to initialize lgmp host");
        ret = -1;
        goto destroy_ctx;
    }

    #define timespecdiff(_start, _end) \
    (((_end).tv_sec - (_start).tv_sec) * 1000000000 + \
    ((_end).tv_nsec - (_start).tv_nsec))

    // Request 100 frames from the first display in the list
    printf( "\"Frame\",\"Size\",\"Request (ms)\",\"Frame Time (ms)\""
            ",\"Speed (Gbit/s)\",\"Framerate (Hz)\"\n");
    struct timespec tstart, tend;

    for (int f = 0; f < 100; f++)
    {
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
        struct fi_cq_data_entry de;
        struct fi_cq_err_entry err;
        ret = trfGetRecvProgress(ctx->lp_client.client_ctx, &de, &err, 1);
        if (ret < 0)
        {
            printf("Unable to get receive progress: error %s\n", 
                   strerror(-ret));
            return -1;
        }
        clock_gettime(CLOCK_MONOTONIC, &tend);
        double tsd2 = timespecdiff(tstart, tend) / 1000000.0;

        if(!lpWriteFrame(ctx,displays))
        {
            lp__log_error("Unable to write frame data");
            ret = -1;
            goto destroy_ctx;
        }
        
        printf("%d,%ld,%f,%f,%f,%f\n",
               f, trfGetDisplayBytes(displays), tsd1, tsd2, 
               ((double) trfGetDisplayBytes(displays)) / tsd2 / 1e5,
               1000.0 / tsd2);
        displays->frame_cntr++;
    }

    

destroy_ctx:
    lpDestroyContext(ctx);
    return ret;
}