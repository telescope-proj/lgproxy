#include "lp_msg.h"

int lpKeepAlive(PTRFContext ctx)
{
    if (!ctx)
    {
        return -EINVAL;
    }
    lp__log_trace("Keeping connection alive...");

    LpMsg__MessageWrapper mw = LP_MSG__MESSAGE_WRAPPER__INIT;
    LpMsg__KeepAlive ka = LP_MSG__KEEP_ALIVE__INIT;
    mw.wdata_case = LP_MSG__MESSAGE_WRAPPER__WDATA_KA;
    mw.ka = &ka;
    ka.info = 0;

    ssize_t dsize = trfMsgPackProtobuf((ProtobufCMessage *) &mw, 
                ctx->xfer.fabric->msg_mem.size,
                trfMemPtr(&ctx->xfer.fabric->msg_mem));
    if (dsize < 0)
    {
        lp__log_error("unable to encode data");
        return dsize;
    }

    lp__log_debug("Sending %ld bytes over fabric", dsize);

    return trfFabricSend(ctx, &ctx->xfer.fabric->msg_mem, 
                    trfMemPtr(&ctx->xfer.fabric->msg_mem),
                    dsize, ctx->xfer.fabric->peer_addr, 
                    ctx->opts);
}