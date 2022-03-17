#include "lp_write.h"

int lpInitHost(PLPContext ctx, PTRFDisplay display)
{
    if (!ctx)
    {
        return -EINVAL;
    }
    int ret = 0;
    int fd = open(ctx->shm, O_RDWR | O_CREAT, (mode_t) 0600);
    if (fd < 0)
    {
        lp__log_error("Unable to open shared file: %s", strerror(errno));
        ret = -errno;
        goto out;
    }
    if(ftruncate(fd, ctx->ram_size) != 0)
    {
        lp__log_error("Unable to truncate shm file: %s", strerror(errno));
        ret = -errno;
        goto close_fd;
    }
    ctx->ram = mmap(0, ctx->ram_size, PROT_READ | PROT_WRITE, MAP_SHARED, 
                fd, 0);
    if (!ctx->ram_size)
    {
        lp__log_error("Unable to map shared memory: %s", strerror(errno));
        ret = -errno;
        goto close_fd;
    }

    LGMP_STATUS status;

    while ((status = lgmpHostInit(ctx->ram, ctx->ram_size, 
                &ctx->lp_host.lgmp_host, 0, NULL)) != LGMP_OK)
    {
        lp__log_error("Unable to init host");
        ret = -1;
        goto close_fd;
    }
    
    struct LGMPQueueConfig frameQueueConfig = {
        .queueID        = LGMP_Q_FRAME,
        .numMessages    = LGMP_Q_FRAME_LEN,
        .subTimeout     = 1000,
    };
    
    if ((status = lgmpHostQueueNew(ctx->lp_host.lgmp_host, frameQueueConfig, 
                &ctx->lp_host.host_q)) != LGMP_OK)
    {
        lp__log_error("Unabel to create new host queue: %s", 
            lgmpStatusString(status));
        ret = -1;
        goto close_fd;
    }

    ssize_t dispsize = trfGetDisplayBytes(display);

    for (int i = 0; i < LGMP_Q_FRAME_LEN; ++i )
    {
        if ((status = lgmpHostMemAllocAligned(ctx->lp_host.lgmp_host, dispsize,
                trf__GetPageSize(), &ctx->lp_host.frame_memory[i])))
        {
            lp__log_error("lgmpHostMemAllocAligned Failed: %s", 
                lgmpStatusString(status));
            ret = -1;
            goto close_fd;
        }
    }

    return 0;

close_fd:
    close(fd);
out:
    return ret;
}


bool lpWriteFrame(PLPContext ctx, PTRFDisplay display)
{
    bool repeatFrame = false;
    
    while (ctx->state == LP_STATE_RUNNING && 
        lgmpHostQueuePending(ctx->lp_host.host_q) == LGMP_Q_FRAME_LEN)
    {
        usleep(1);
        continue;
    }
    if (ctx->state != LP_STATE_RUNNING)
    {
        return false;
    }

    LGMP_STATUS status;

    if (repeatFrame)
    {
        status = lgmpHostQueuePost(ctx->lp_host.host_q, 0, 
            ctx->lp_host.frame_memory[ctx->lp_host.frame_index]);
        if (status != LGMP_OK)
        {
            lp__log_error("Failed lgmpHostQueuePost: %d", 
                lgmpStatusString(status));
            return false;
        }
    }
    if (++ctx->lp_host.frame_index == LGMP_Q_FRAME_LEN)
    {
        ctx->lp_host.frame_index = 0;
    }
    
    KVMFRFrame *metadata = (KVMFRFrame *)display->fb_addr;
    metadata->type = lpTrftoLGFormat(display->format);

    memcpy(ctx->lp_host.frame_memory[ctx->lp_host.frame_index], 
                display->fb_addr, trfGetDisplayBytes(display));
    return 0;
}