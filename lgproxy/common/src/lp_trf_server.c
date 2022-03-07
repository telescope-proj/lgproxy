#include "lp_trf_server.h"


int lpTrfServerInit(PLPContext ctx, char * host, char * port){
    int ret = 0;

    ctx->server_ctx = trfAllocContext(); //Initialize Server Context
    if (!ctx->server_ctx){
        lp__log_error("Unable to allocate server context");
        return -ENOMEM;
    }

    if((ret = trfNCServerInit(ctx->server_ctx, host, port)) < 0)
    {
        lp__log_error("Unable to Initialize Server");
        goto destroy_server;
    }

destroy_server:
    trfDestroyContext(ctx->server_ctx);
    return ret;
}

