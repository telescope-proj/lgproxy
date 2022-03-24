#include "lp_types.h"


PLPContext lpAllocContext(){
    PLPContext ctx = calloc(1, sizeof(* ctx));
    return ctx;
}

void lpDestroyContext(PLPContext ctx){
    if (ctx->lp_client.lgmp_client)
    {
        lgmpClientFree(&ctx->lp_client.lgmp_client);
    }
    if (ctx->lp_host.lgmp_host)
    {
        lgmpHostFree(&ctx->lp_host.lgmp_host);
    }
    if (ctx->lp_client.client_ctx)
    {
        trfDestroyContext(ctx->lp_client.client_ctx);
    }
    if (ctx->lp_host.server_ctx)
    {
        trfDestroyContext(ctx->lp_host.server_ctx);
    }

    munmap(ctx->ram, ctx->ram_size);
    if (ctx->shmFile)
    {
        close(ctx->shmFile);
    }
}