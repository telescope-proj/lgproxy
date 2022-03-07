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
            case 's':
                ctx->ram_size = (uint32_t) atoi(optarg);
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

    if((ret = trfNCAccept(ctx->server_ctx, &ctx->client_ctx)) < 0)
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

    KVMFRFrame * metadata = calloc(1, sizeof(struct KVMFRFrame)); 
    if (!metadata)
    {
        lp__log_error("Unable to allocate memory");
        ret = -ENOMEM;
        goto destroy_ctx;
    }


    FrameBuffer * fb = calloc(1, 4UL);
    if (!fb)
    {
        lp__log_error("Unable to allocate memory");
        free(metadata);
        ret = -ENOMEM;
        goto destroy_ctx;
    }

    // Get the first frame from the host so we have metadata

    if ((ret = lpGetFrame(ctx, metadata, fb)) < 0)
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
    
    ret = trfBindDisplayList(ctx->client_ctx, displays); //Bind display list to client context
    if (ret < 0)
    {
        trf__log_error("Unable to bind display list");
        ret = -1;
        goto destroy_ctx;
    }

    uint64_t processed;
    TrfMsg__MessageWrapper *msg = NULL;
    while (1){
        ret = trfGetMessageAuto(ctx->client_ctx, TRFM_SET_DISP, 
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
    
    ret = trfGetMessageAuto(ctx->client_ctx, 0, &processed, (void **) &msg);
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


    return 0;


destroy_ctx:
    lpDestroyContext(ctx);
    return ret;
}