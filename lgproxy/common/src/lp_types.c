#include "lp_types.h"

int lpPollMsg(PLPContext ctx, TrfMsg__MessageWrapper ** msg)
{
    struct fi_cq_data_entry de;
    struct fi_cq_err_entry err;
    ssize_t ret;

    ret = trfFabricPollRecv(ctx->lp_client.client_ctx, &de, &err, 0, 0, NULL, 1);
    if (ret == 0 || ret == -EAGAIN)
    {
        return -EAGAIN;
    }
    if (ret < 0)
    {
        lp__log_error("Unable to poll CQ: %s", fi_strerror(-ret));
        return ret;
    }
    void *msgmem = trfMemPtr(&ctx->lp_client.client_ctx->xfer.fabric->msg_mem);
    ret = trfMsgUnpack(msg, 
                       trfMsgGetPackedLength(msgmem),
                       trfMsgGetPayload(msgmem));
    if (ret < 0)
    {
        printf("Unable to unpack: %s\n", 
            strerror(-ret));
        return ret;
    }
    return 0;
}


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