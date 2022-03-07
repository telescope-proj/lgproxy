#include "lp_trf.h"


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

    if((ret = trfNCAccept(ctx->server_ctx, &ctx->client_ctx)) < 0)
    {
        lp__log_error("Unable to Accept Client Connection");
        goto destroy_server;
    }

destroy_server:
    trfDestroyContext(ctx->server_ctx);
    return ret;
}

int lpTrfClientInit(PLPContext ctx, char * host, char * port){
    int ret = 0;
    ctx->client_ctx = trfAllocContext(); //Initialize Client Context
    if (!ctx->client_ctx){
        lp__log_error("Unable to allocate client context");
        return -ENOMEM;
    }

    if ((ret = trfNCClientInit(ctx->client_ctx, host, port)) < 0)
    {
        lp__log_error("Unable to Initialize Client");
        goto destroy_client;
    }
    
    trf__log_debug("Client Initialized");
    

destroy_client:
    trfDestroyContext(ctx->client_ctx);
    return ret;
}