#include "lp_utils.h"

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
        lp__log_error("Unable to unpack: %s\n", 
            strerror(-ret));
        return ret;
    }
    return 0;
}

uint64_t lpParseMemString(char * data)
{
    char multiplier = data[strlen(data)-1];
    uint64_t b = atoi(data);
    if (multiplier >= '0' && multiplier <= '9')
    {
        return b;
    }
    else
    {
        switch (multiplier)
        {
            case 'K':
            case 'k':
                return b * 1024;
            case 'M':
            case 'm':
                return b * 1024 * 1024;
            case 'G':
            case 'g':
                return b * 1024 * 1024 * 1024;
            default:
                return -1;
        }
    }
}

bool lpShouldTruncate(PLPContext ctx)
{
    struct stat filestat;
    if (stat(ctx->shm, &filestat) < 0)
    {
        lp__log_error("Unable to get stats on shm file");
    }
    if (strncmp(ctx->shm, "/dev/kvmfr", sizeof("/dev/kvmfr")-1) == 0)
    {
        return false;
    }
    if (ctx->ram_size > filestat.st_size)
    {
        return true;
    }
    return false;
}