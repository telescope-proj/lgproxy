#include "lp_write.h"
#include "version.h"

typedef struct KVMFRUserData
{
  size_t    size;
  size_t    used;
  uint8_t * data;
}
KVMFRUserData;

static bool appendData(KVMFRUserData * dst, const void * src, const size_t size)
{
  if (size > dst->size - dst->used)
  {
    size_t newSize = dst->size + (1024 > size ? size : 1024);
    dst->data = realloc(dst->data, newSize);
    if (!dst->data)
    {
      lp__log_error("Out of memory!");
      return false;
    }

    memset(dst->data + dst->size, 0, newSize - dst->size);
    dst->size = newSize;
  }

  memcpy(dst->data + dst->used, src, size);
  dst->used += size;
  return true;
}

static bool newKVMFRData(KVMFRUserData * dst)
{
  {
    KVMFR kvmfr =
    {
      .magic    = KVMFR_MAGIC,
      .version  = KVMFR_VERSION,
      .features = 0
    };
    strncpy(kvmfr.hostver, LP_BUILD_VERSION, sizeof(kvmfr.hostver) - 1);
    appendData(dst, &kvmfr, sizeof(kvmfr));
  }

  {
    int cpus        = 1;
    int cores       = 1;
    int sockets     = 1;
    char model[]    = "Virtual CPU";

    KVMFRRecord_VMInfo vmInfo =
    {
      .cpus    = cpus,
      .cores   = cores,
      .sockets = sockets,
    };

    snprintf(vmInfo.capture, sizeof(vmInfo.capture) - 1, "LibTRF %d.%d.%d",
             TRF_API_MAJOR, TRF_API_MINOR, TRF_API_PATCH);

    const int modelLen = strlen(model) + 1;
    const KVMFRRecord record =
    {
      .type = KVMFR_RECORD_VMINFO,
      .size = sizeof(vmInfo) + modelLen
    };

    if (!appendData(dst, &record, sizeof(record)) ||
        !appendData(dst, &vmInfo, sizeof(vmInfo)) ||
        !appendData(dst, model  , modelLen      ))
      return false;
  }

  {
    KVMFRRecord_OSInfo osInfo =
    {
      .os = KVMFR_OS_OTHER
    };

    const char * osName = "MS DOS Executive 6.22";
    if (!osName)
      osName = "";
    const int osNameLen = strlen(osName) + 1;

    KVMFRRecord record =
    {
      .type = KVMFR_RECORD_OSINFO,
      .size = sizeof(osInfo) + osNameLen
    };

    if (!appendData(dst, &record, sizeof(record)) ||
        !appendData(dst, &osInfo, sizeof(osInfo)) ||
        !appendData(dst, osName , osNameLen))
      return false;
  }

  return true;
}

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
    
    bool truncFile = lpShouldTruncate(ctx);
    if (truncFile)
    {
        if(ftruncate(fd, ctx->ram_size) != 0)
        {
            lp__log_error("Unable to truncate shm file: %s", strerror(errno));
            ret = -errno;
            goto close_fd;
        }
    }

    ctx->ram = mmap(0, ctx->ram_size, PROT_READ | PROT_WRITE, MAP_SHARED, 
                fd, 0);
    if (!ctx->ram)
    {
        lp__log_error("Unable to map shared memory: %s", strerror(errno));
        ret = -errno;
        goto close_fd;
    }
    
    lp__log_debug("Creating KVMFR data...");

    KVMFRUserData udata = { 0 };
    if (!newKVMFRData(&udata))
    {
        lp__log_error("KVMFR data init failed!");
        return -EINVAL;
    }

    lp__log_debug("udata: %p (%d/%d)", udata.data, udata.used, udata.size);

    LGMP_STATUS status;
    while ((status = lgmpHostInit(ctx->ram, ctx->ram_size, 
                &ctx->lp_client.lgmp_host, udata.used, udata.data)) != LGMP_OK)
    {
        lp__log_error("Unable to init host");
        ret = -1;
        goto free_udata;
    }
    lp__log_trace("lgmpHostInit: %s", lgmpStatusString(status));

    struct LGMPQueueConfig frameQueueConfig = {
        .queueID        = LGMP_Q_FRAME,
        .numMessages    = LGMP_Q_FRAME_LEN,
        .subTimeout     = 1000,
    };

    struct LGMPQueueConfig pointerQueueConfig = {
        .queueID        = LGMP_Q_POINTER,
        .numMessages    = LGMP_Q_POINTER_LEN,
        .subTimeout     = 1000,
    };
    
    if ((status = lgmpHostQueueNew(ctx->lp_client.lgmp_host, frameQueueConfig, 
                &ctx->lp_client.host_q)) != LGMP_OK)
    {
        lp__log_error("Unable to create new host queue: %s", 
            lgmpStatusString(status));
        ret = -1;
        goto free_udata;
    }

    if ((status = lgmpHostQueueNew(ctx->lp_client.lgmp_host, pointerQueueConfig,
                &ctx->lp_client.pointer_q)) != LGMP_OK)
    {
        lp__log_error("Unable to create new pointer queue: %s",
                lgmpStatusString(status));
        ret = -1;
        goto free_udata;
    }

    for(int i = 0; i < LGMP_Q_POINTER_LEN; ++i)
    {
        if ((status = lgmpHostMemAlloc(ctx->lp_client.lgmp_host, 
            sizeof(KVMFRCursor), &ctx->lp_client.pointer_memory[i])) != LGMP_OK)
        {
        lp__log_error("lgmpHostMemAlloc Failed (Pointer): %s", 
            lgmpStatusString(status));
        goto free_udata;
        }
        memset(lgmpHostMemPtr(ctx->lp_client.pointer_memory[i]), 0, 
                sizeof(KVMFRCursor));
    }

    for(int i = 0; i < POINTER_SHAPE_BUFFERS; ++i)
    {
        if ((status = lgmpHostMemAlloc(ctx->lp_client.lgmp_host, 
            MAX_POINTER_SIZE, &ctx->lp_client.cursor_shape[i])) != LGMP_OK)
        {
        lp__log_error("lgmpHostMemAlloc Failed (Pointer Shapes): %s", 
                lgmpStatusString(status));
        goto free_udata;
        }
        memset(lgmpHostMemPtr(ctx->lp_client.cursor_shape[i]), 
                0, MAX_POINTER_SIZE);
    }


    ssize_t dispsize = trfGetDisplayBytes(display);
    for (int i = 0; i < LGMP_Q_FRAME_LEN; ++i )
    {
        if ((status = lgmpHostMemAllocAligned(ctx->lp_client.lgmp_host, dispsize,
                trf__GetPageSize(), &ctx->lp_client.frame_memory[i])) != LGMP_OK)
        {
            lp__log_error("lgmpHostMemAllocAligned Failed: %s", 
                lgmpStatusString(status));
            ret = -1;
            goto free_udata;
        }
    }

    return 0;

free_udata:
    free(udata.data);
close_fd:
    close(fd);
out:
    return ret;
}

LGMP_STATUS lpKeepLGMPSessionAlive(PLPContext ctx)
{
    LGMP_STATUS status;
    status = lgmpHostProcess(ctx->lp_client.lgmp_host);
    if (status != LGMP_OK)
    {
        lp__log_error("lgmpHostProcess failed: %s", lgmpStatusString(status));
    }
    return status;
}

int lpSignalFrameDone(PLPContext ctx, PTRFDisplay disp)
{
    if (!ctx || !disp)
        return -EINVAL;

    FrameBuffer * fb = trfGetFBPtr(disp) - FrameBufferStructSize;
    framebuffer_set_write_ptr(fb, trfGetDisplayBytes(disp));
    return 0;
}

int lpRequestFrame(PLPContext ctx, PTRFDisplay disp)
{
    bool repeatFrame = false;
    while (ctx->state == LP_STATE_RUNNING && 
        lgmpHostQueuePending(ctx->lp_client.host_q) == LGMP_Q_FRAME_LEN)
    {
        usleep(1);
        continue;
    }

    if (ctx->state != LP_STATE_RUNNING)
    {
        return -EAGAIN;
    }

    LGMP_STATUS status;
    status = lgmpHostProcess(ctx->lp_client.lgmp_host);
    if (status != LGMP_OK)
    {
        lp__log_error("lgmpHostProcess failed: %s", lgmpStatusString(status));
        return -1;
    }

    if (repeatFrame)
    {
        status = lgmpHostQueuePost(
            ctx->lp_client.host_q, 0,
            ctx->lp_client.frame_memory[ctx->lp_client.frame_index]);
        if (status != LGMP_OK)
        {
            lp__log_error("Failed lgmpHostQueuePost: %s", 
                lgmpStatusString(status));
            return -ENOBUFS;
        }
    }

    lgmpHostQueueNewSubs(ctx->lp_client.host_q);
    KVMFRFrame *fi = lgmpHostMemPtr(ctx->lp_client.frame_memory[ctx->lp_client.frame_index]);

    uint8_t comp = trfTextureIsCompressed(disp->format);
    

    fi->rotation         = FRAME_ROT_0;
    fi->frameSerial      = disp->frame_cntr;
    fi->formatVer        = 1;
    fi->damageRectsCount = 0;
    fi->type             = lpTrftoLGFormat(disp->format);
    fi->height           = disp->height;
    fi->realHeight       = disp->height;
    fi->offset           = trf__GetPageSize() - FrameBufferStructSize;
    fi->stride           = !comp ? disp->width : 0;
    fi->pitch            = !comp ? 
                           trfGetTextureBytes(disp->width, 1, disp->format) :0;
    fi->width            = disp->width;

    lp__log_trace("Display size received: %d x %d", fi->width, fi->height);
    lp__log_trace("Display Type: %d", lpTrftoLGFormat(disp->format));

    disp->fb_offset = ((uint8_t *) fi - (uint8_t *) ctx->ram) + fi->offset
                       + FrameBufferStructSize;
    
    FrameBuffer * fb = (FrameBuffer *) (((uint8_t *) fi) + fi->offset);
    framebuffer_prepare(fb);

    lp__log_trace("Absolute memory position: %p. Relative offset: %lu", 
                  framebuffer_get_data(fb), disp->fb_offset);
    
    if (trfGetFBPtr(disp) != framebuffer_get_data(fb))
    {
        lp__log_error("Address mismatch! Looking Glass: %p, LibTRF: %p",
                      framebuffer_get_data(fb), trfGetFBPtr(disp));
    }

    if ((status = lgmpHostQueuePost(ctx->lp_client.host_q, 0, 
            ctx->lp_client.frame_memory[ctx->lp_client.frame_index])) != LGMP_OK)
    {
        lp__log_error("Unable to post queue: %s", lgmpStatusString(status));
        return true;
    }
    lp__log_debug("Display offset: %lu", disp->fb_offset);
    return 0;
} 

int lpUpdateCursorPos(PLPContext ctx, KVMFRCursor * cur, uint32_t curShapeSize, 
                uint32_t flags)
{
    if (!ctx || !cur)
    {
        return -EINVAL;
    }
    
    PLGMPMemory mem = ctx->lp_client.cursor_shape[0];
    KVMFRCursor *tmpCur = lgmpHostMemPtr(mem);
    memcpy((void *) tmpCur, (void *) cur, curShapeSize + sizeof(KVMFRCursor));
    
    int ret = lpPostCursor(ctx,flags,mem);
    if (ret < 0)
        return ret;

    ctx->lp_client.pointer_shape_valid = curShapeSize ? true : false;
    return 0;
}

int lpPostCursor(PLPContext ctx, uint32_t flags, PLGMPMemory mem)
{
    LGMP_STATUS status;
    while ((status = lgmpHostQueuePost(ctx->lp_client.pointer_q, flags, mem)) 
                != LGMP_OK)
    {
        if (status == LGMP_ERR_QUEUE_FULL)
        {
            return -EAGAIN;
        }
        lp__log_error("lgmpHostQueuePost (Pointer) failed: %s", 
                lgmpStatusString(status));
        return -1;
    }
    return 0;
}