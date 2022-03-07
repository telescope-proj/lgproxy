#include "lp_types.h"


PLPContext lpAllocContext(){
    PLPContext ctx = calloc(1, sizeof(* ctx));
    return ctx;
}

void lpDestroyContext(PLPContext ctx){
    if (ctx->lgmp_client)
    {
        lgmpClientFree(&ctx->lgmp_client);
    }
    if (ctx->lgmp_host)
    {
        lgmpHostFree(&ctx->lgmp_host);
    }
    trfDestroyContext(ctx->client_ctx);
    trfDestroyContext(ctx->server_ctx);

    munmap(ctx->ram, ctx->ram_size);
    close(ctx->shmFile);
}