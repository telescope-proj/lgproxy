#include "lp_trf_server.h"


int lpTrfServerInit(PLPContext ctx, char * host, char * port){
    int ret = 0;

    ctx->lp_host.server_ctx = trfAllocContext(); //Initialize Server Context
    if (!ctx->lp_host.server_ctx){
        lp__log_error("Unable to allocate server context");
        return -ENOMEM;
    }

    if((ret = trfNCServerInit(ctx->lp_host.server_ctx, host, port)) < 0)
    {
        lp__log_error("Unable to Initialize Server");
        goto destroy_server;
    }

    return 0;

destroy_server:
    trfDestroyContext(ctx->lp_host.server_ctx);
    return ret;
}

