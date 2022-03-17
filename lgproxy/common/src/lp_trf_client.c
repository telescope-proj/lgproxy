#include "lp_trf_client.h"


int lpTrfClientInit(PLPContext ctx, char * host, char * port){
    int ret = 0;
    ctx->lp_client.client_ctx = trfAllocContext(); //Initialize Client Context
    if (!ctx->lp_client.client_ctx){
        lp__log_error("Unable to allocate client context");
        return -ENOMEM;
    }

    if ((ret = trfNCClientInit(ctx->lp_client.client_ctx, host, port)) < 0)
    {
        lp__log_error("Unable to Initialize Client");
        goto destroy_client;
    }
    
    trf__log_debug("Client Initialized");
    
    return 0;
    
destroy_client:
    trfDestroyContext(ctx->lp_client.client_ctx);
    return ret;
}