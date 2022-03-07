#include"lp_retrieve.h"

int lpInitLgmpClient(PLPContext ctx)
{
    int ret = 0;
    int fd = open(ctx->shm, O_RDWR, (mode_t)0600);
    if(fd < 0)
    {
        lp__log_error("Unable to open shared file: %s", strerror(errno));
        ret = -errno;
        goto out;
    }
    ctx->ram = mmap(0, ctx->ram_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (!ctx->ram)
    {
        lp__log_error("Unable to map shared memory: %s", strerror(errno));
        ret = -errno;
        goto close_fd;
    }
    LGMP_STATUS status;
    while ((status = lgmpClientInit(ctx->ram,ctx->ram_size, ctx->lgmp_client)) != LGMP_OK)
    {
        lp__log_error("LGMP Client Init failure; %s\n", lgmpStatusString(status));
        goto unmap;
    }
    uint32_t udataSize;
    uint8_t *udata;
    while((status = lgmpClientSessionInit(ctx->lgmp_client, &udataSize, &udata))
     != LGMP_OK){
        usleep(100000);
        lp__log_error("lgmpSession Failed:", lgmpStatusString(status));
     }
    
    trfSleep(200); // Wait for host to update


unmap:
    munmap(ctx->ram,ctx->ram_size);
close_fd:
    close(fd);
out: 
    return ret;
}

int lpInitSession(PLPContext ctx)
{
    LGMP_STATUS status;
    uint32_t udataSize;
    KVMFR *udata;
    int waitCount = 0;

    int retry = 0; //! Change to use states 
    while(retry < 20)
    {
        status = lgmpClientSessionInit(ctx->
        lgmp_client, &udataSize, (uint8_t **) &udata);
        switch (status)
        {
        case LGMP_OK:
            ctx->state = LP_STATE_RUNNING;
            break;
        case LGMP_ERR_INVALID_VERSION:
            lp__log_debug("Incompatible LGMP Version"
                    "The host application is not compatible with this client"
                    "Please download and install the matching version");
            while(retry < 20 && lgmpClientSessionInit(ctx->lgmp_client, 
                        &udataSize, (uint8_t **) &udata) != LGMP_OK){
                            trfSleep(1000);
                        }
            if(retry >= 20){
                ctx->state = LP_STATE_STOP;
                lp__log_debug("Incompatible LGMP Versions between client and host");
                return -1;
            }
            continue;
        case LGMP_ERR_INVALID_SESSION:
        case LGMP_ERR_INVALID_MAGIC:
        {
            if(waitCount++ == 0){
                ctx->state = LP_STATE_STOP;
                lp__log_error("Host application does not seem to be running");
            }
        }
        if(waitCount++ == 30)
        {
            lp__log_debug(
              "Host Application Not Running",
              "It seems the host application is not running or your\n"
              "virtual machine is still starting up\n"
              "\n"
              "If the the VM is running and booted please check the\n"
              "host application log for errors. You can find the\n"
              "log through the shortcut in your start menu\n"
              "\n"
              "Continuing to wait...");
        }
        trfSleep(1000);
        continue;
        default:
            ctx->state = LP_STATE_STOP;
            lp__log_error("lgmpClient Session Init failed: %s", lgmpStatusString(status));
            return -1;
        }
        if(ctx->state != LP_STATE_RUNNING)
        {
            lp__log_error("Unable to initialize session");
            return -1;
        }
    }
}
